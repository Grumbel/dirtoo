// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/trash.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>
#include <unordered_set>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef RENAME_NOREPLACE
#  define RENAME_NOREPLACE (1 << 0)
#endif

namespace dirops {
namespace {

namespace fs = std::filesystem;

Error errno_error(int err, const fs::path& path, std::string message)
{
  return Error{std::error_code(err, std::generic_category()), path, std::move(message)};
}

/// lstat wrapper: true on success.
bool lstat_path(const fs::path& p, struct stat* st)
{
  return ::lstat(p.c_str(), st) == 0;
}

/// Device of @p p, or of its closest existing ancestor (the home trash may
/// not have been created yet).
std::optional<dev_t> device_of_existing(fs::path p)
{
  struct stat st {};
  while (true) {
    if (lstat_path(p, &st)) {
      return st.st_dev;
    }
    const fs::path parent = p.parent_path();
    if (parent == p || parent.empty()) {
      return std::nullopt;
    }
    p = parent;
  }
}

fs::path absolute_clean(const fs::path& p)
{
  std::error_code ec;
  fs::path abs = fs::absolute(p, ec);
  if (ec) {
    abs = p;
  }
  abs = abs.lexically_normal();
  // "dir/" normalises to "dir/" (empty filename): drop the trailing slash.
  while (abs.has_relative_path() && abs.filename().empty()) {
    abs = abs.parent_path();
  }
  return abs;
}

bool is_within(const fs::path& inner, const fs::path& outer)
{
  auto in = inner.begin();
  for (auto out = outer.begin(); out != outer.end(); ++out, ++in) {
    if (in == inner.end() || *in != *out) {
      return false;
    }
  }
  return true;
}

uid_t effective_uid(const TrashOptions& t)
{
  return t.uid.value_or(::geteuid());
}

// --- percent-encoding of the Path= value -------------------------------

std::string percent_encode_path(const std::string& path)
{
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(path.size() + 8);
  for (const unsigned char c : path) {
    const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                            || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_'
                            || c == '~' || c == '/';
    if (unreserved) {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += kHex[c >> 4];
      out += kHex[c & 0x0F];
    }
  }
  return out;
}

int hex_value(char c)
{
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

std::string percent_decode(const std::string& in)
{
  std::string out;
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size(); ++i) {
    if (in[i] == '%' && i + 2 < in.size() + 0 && hex_value(in[i + 1]) >= 0
        && hex_value(in[i + 2]) >= 0) {
      out += static_cast<char>((hex_value(in[i + 1]) << 4) | hex_value(in[i + 2]));
      i += 2;
    } else {
      out += in[i];
    }
  }
  return out;
}

std::string deletion_date_now()
{
  const std::time_t now = std::time(nullptr);
  std::tm tm {};
  ::localtime_r(&now, &tm);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
  return buf;
}

// --- trash directory selection -----------------------------------------

/// Create @p dir (0700) if missing; true if it is usable afterwards.
bool ensure_private_dir(const fs::path& dir)
{
  if (::mkdir(dir.c_str(), 0700) != 0 && errno != EEXIST) {
    return false;
  }
  struct stat st {};
  return lstat_path(dir, &st) && S_ISDIR(st.st_mode);
}

/// Create the trash directory with its files/ and info/ children.
bool ensure_trash_dir(const fs::path& trash)
{
  std::error_code ec;
  fs::create_directories(trash.parent_path(), ec);  // e.g. ~/.local/share
  return ensure_private_dir(trash) && ensure_private_dir(trash / "files")
         && ensure_private_dir(trash / "info");
}

/// "$topdir/.Trash" qualifies when it is a real directory (not a symlink)
/// with the sticky bit set (trash-spec, "Trash directories").
bool shared_trash_usable(const fs::path& topdir)
{
  struct stat st {};
  const fs::path shared = topdir / ".Trash";
  return lstat_path(shared, &st) && S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)
         && (st.st_mode & S_ISVTX) != 0;
}

fs::path volume_trash_candidate(const fs::path& topdir, uid_t uid, bool shared)
{
  return shared ? topdir / ".Trash" / std::to_string(uid)
                : topdir / (".Trash-" + std::to_string(uid));
}

struct ChosenTrash {
  fs::path dir;     ///< the trash directory
  fs::path topdir;  ///< empty for the home trash
};

std::expected<ChosenTrash, Error> choose_trash(const fs::path& abs, dev_t file_dev,
                                               const TrashOptions& opts)
{
  const fs::path home = opts.home_trash.value_or(home_trash_dir());
  const auto home_dev = device_of_existing(home);
  if (home_dev && *home_dev == file_dev) {
    if (!ensure_trash_dir(home)) {
      return std::unexpected(errno_error(errno != 0 ? errno : EACCES, home,
                                         "cannot create the home trash directory"));
    }
    return ChosenTrash{home, {}};
  }

  // Another filesystem: find its top directory (the highest ancestor that is
  // still on the same device).
  struct stat st {};
  fs::path top = abs.parent_path();
  if (opts.topdir_override) {
    top = *opts.topdir_override;
  } else {
    while (top.has_parent_path() && top != top.root_path()) {
      const fs::path up = top.parent_path();
      if (!lstat_path(up, &st) || st.st_dev != file_dev) {
        break;
      }
      top = up;
    }
  }

  const uid_t uid = effective_uid(opts);
  if (shared_trash_usable(top)) {
    const fs::path dir = volume_trash_candidate(top, uid, /*shared=*/true);
    if (ensure_trash_dir(dir)) {
      return ChosenTrash{dir, top};
    }
  }
  const fs::path dir = volume_trash_candidate(top, uid, /*shared=*/false);
  if (ensure_trash_dir(dir)) {
    // The directory may pre-exist: it must be ours and not a symlink.
    if (lstat_path(dir, &st) && S_ISDIR(st.st_mode) && st.st_uid == uid) {
      return ChosenTrash{dir, top};
    }
  }
  return std::unexpected(errno_error(EXDEV, abs,
                                     "no usable trash directory on this filesystem (" +
                                         top.string() + ")"));
}

// --- trashinfo ----------------------------------------------------------

struct ParsedInfo {
  std::string path;  ///< decoded Path=
  std::string date;
  bool ok = false;
};

ParsedInfo read_info(const fs::path& info_file)
{
  ParsedInfo info;
  std::ifstream in(info_file, std::ios::binary);
  if (!in) {
    return info;
  }
  std::string line;
  bool in_group = false;
  std::size_t total = 0;
  while (std::getline(in, line) && (total += line.size()) < 64 * 1024) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.starts_with("[")) {
      in_group = line == "[Trash Info]";
      continue;
    }
    if (!in_group) {
      continue;
    }
    const auto eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    const std::string key = line.substr(0, eq);
    const std::string value = line.substr(eq + 1);
    if (key == "Path" && info.path.empty()) {
      info.path = percent_decode(value);
    } else if (key == "DeletionDate" && info.date.empty()) {
      info.date = value;
    }
  }
  info.ok = !info.path.empty();
  return info;
}

/// Cut @p name to at most @p max bytes at a UTF-8 character boundary.
std::string clip_utf8(const std::string& name, std::size_t max)
{
  if (name.size() <= max) {
    return name;
  }
  std::size_t cut = max;
  while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0) == 0x80) {
    --cut;  // do not split a multi-byte sequence
  }
  return name.substr(0, cut);
}

int rename_noreplace(const char* from, const char* to)
{
#ifdef SYS_renameat2
  if (::syscall(SYS_renameat2, AT_FDCWD, from, AT_FDCWD, to, RENAME_NOREPLACE) == 0) {
    return 0;
  }
  if (errno != ENOSYS && errno != EINVAL) {
    return -1;  // EEXIST and real errors
  }
  // Filesystem or kernel without RENAME_NOREPLACE: fall back to rename(),
  // after the caller's lstat() check.
#endif
  struct stat st {};
  if (::lstat(to, &st) == 0) {
    errno = EEXIST;
    return -1;
  }
  return ::rename(from, to);
}

// --- discovering trash directories ---------------------------------------

std::string unescape_mount_field(const std::string& s)
{
  std::string out;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 3 < s.size() + 0 && std::isdigit(static_cast<unsigned char>(s[i + 1]))) {
      out += static_cast<char>(std::stoi(s.substr(i + 1, 3), nullptr, 8));
      i += 3;
    } else {
      out += s[i];
    }
  }
  return out;
}

std::vector<fs::path> system_mount_points()
{
  std::vector<fs::path> out;
  std::ifstream in("/proc/self/mounts");
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::string dev;
    std::string mp;
    if (ls >> dev >> mp) {
      out.emplace_back(unescape_mount_field(mp));
    }
  }
  return out;
}

struct TrashLocation {
  fs::path dir;
  fs::path topdir;
};

std::vector<TrashLocation> all_trash_dirs(const TrashOptions& opts)
{
  std::vector<TrashLocation> out;
  std::set<fs::path> seen;
  auto add = [&](const fs::path& dir, const fs::path& top) {
    struct stat st {};
    if (lstat_path(dir, &st) && S_ISDIR(st.st_mode) && seen.insert(dir).second) {
      out.push_back({dir, top});
    }
  };
  add(opts.home_trash.value_or(home_trash_dir()), {});
  const uid_t uid = effective_uid(opts);
  const std::vector<fs::path> mounts = opts.mount_points.value_or(system_mount_points());
  for (const auto& mp : mounts) {
    if (shared_trash_usable(mp)) {
      add(volume_trash_candidate(mp, uid, true), mp);
    }
    add(volume_trash_candidate(mp, uid, false), mp);
  }
  return out;
}

bool safe_entry_name(const std::string& name)
{
  return !name.empty() && name != "." && name != ".." && name.find('/') == std::string::npos;
}

bool cancelled(const Options& options)
{
  return options.is_cancelled && options.is_cancelled();
}

} // namespace

fs::path home_trash_dir()
{
  const char* data = std::getenv("XDG_DATA_HOME");
  if (data != nullptr && data[0] == '/') {
    return fs::path{data} / "Trash";
  }
  const char* home = std::getenv("HOME");
  return fs::path{home != nullptr ? home : "."} / ".local" / "share" / "Trash";
}

std::expected<TrashEntry, Error> trash_entry(const fs::path& path, const TrashOptions& opts)
{
  if (path.empty()) {
    return std::unexpected(errno_error(EINVAL, path, "empty path"));
  }
  const fs::path abs = absolute_clean(path);
  const fs::path name = abs.filename();
  if (!abs.has_relative_path() || name == "." || name == "..") {
    return std::unexpected(
        errno_error(EINVAL, path, "refusing to trash the root, '.' or '..'"));
  }

  struct stat st {};
  if (!lstat_path(abs, &st)) {
    return std::unexpected(errno_error(errno, abs, "cannot trash: no such file or directory"));
  }
  struct stat parent_st {};
  if (lstat_path(abs.parent_path(), &parent_st) && parent_st.st_dev != st.st_dev) {
    return std::unexpected(errno_error(EBUSY, abs, "cannot trash a mount point"));
  }

  auto chosen = choose_trash(abs, st.st_dev, opts);
  if (!chosen) {
    return std::unexpected(chosen.error());
  }
  // Never trash something that is already in a trash directory.
  if (is_within(abs, chosen->dir) || is_within(abs, chosen->dir.parent_path() / "Trash")) {
    return std::unexpected(errno_error(EINVAL, abs, "item is already in the trash"));
  }

  TrashEntry entry;
  entry.trash_dir = chosen->dir;
  entry.topdir = chosen->topdir;
  entry.original_path = abs;
  entry.deletion_date = deletion_date_now();

  const fs::path recorded = chosen->topdir.empty() ? abs : abs.lexically_relative(chosen->topdir);
  std::string content = "[Trash Info]\nPath=" + percent_encode_path(recorded.string()) +
                        "\nDeletionDate=" + entry.deletion_date + "\n";

  // Names are unique per trash directory. The info file is created with
  // O_EXCL so two processes never pick the same name; the item then moves
  // with a no-replace rename.
  const std::string base = clip_utf8(name.string(), 200);
  for (int n = 1; n < 10000; ++n) {
    const std::string candidate = n == 1 ? base : base + "." + std::to_string(n);
    entry.name = candidate;
    const fs::path info = entry.info_path();
    const fs::path stored = entry.stored_path();

    struct stat existing {};
    if (lstat_path(stored, &existing)) {
      continue;  // a file with that name is already in files/
    }
    const int fd = ::open(info.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0) {
      if (errno == EEXIST) {
        continue;
      }
      return std::unexpected(errno_error(errno, info, "cannot create the trash info file"));
    }
    const bool wrote = ::write(fd, content.data(), content.size()) ==
                       static_cast<ssize_t>(content.size());
    const int close_rc = ::close(fd);
    if (!wrote || close_rc != 0) {
      const int err = errno;
      ::unlink(info.c_str());
      return std::unexpected(errno_error(err, info, "cannot write the trash info file"));
    }
    if (rename_noreplace(abs.c_str(), stored.c_str()) != 0) {
      const int err = errno;
      ::unlink(info.c_str());  // do not leave an info file without an item
      if (err == EEXIST) {
        continue;
      }
      return std::unexpected(errno_error(err, abs, "cannot move the item into the trash"));
    }
    return entry;
  }
  return std::unexpected(errno_error(EEXIST, abs, "could not find a free name in the trash"));
}

OpResult trash_path(const fs::path& path, const TrashOptions& trash, const Options& options)
{
  if (cancelled(options)) {
    return Result{.items = {}, .cancelled = true};
  }
  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = path, .destination = {}});
    return r;
  }
  auto entry = trash_entry(path, trash);
  if (!entry) {
    return std::unexpected(entry.error());
  }
  Result r;
  r.items.push_back(ItemResult{.source = entry->original_path, .destination = entry->stored_path()});
  return r;
}

std::vector<TrashEntry> list_trash(const TrashOptions& opts)
{
  std::vector<TrashEntry> out;
  for (const auto& loc : all_trash_dirs(opts)) {
    std::error_code ec;
    fs::directory_iterator it(loc.dir / "info", ec);
    for (; !ec && it != fs::directory_iterator{}; it.increment(ec)) {
      const fs::path info_file = it->path();
      if (info_file.extension() != ".trashinfo") {
        continue;
      }
      TrashEntry e;
      e.trash_dir = loc.dir;
      e.topdir = loc.topdir;
      e.name = info_file.stem().string();
      struct stat st {};
      if (!safe_entry_name(e.name) || !lstat_path(e.stored_path(), &st)) {
        continue;  // orphaned info file
      }
      const ParsedInfo info = read_info(info_file);
      if (!info.ok) {
        continue;
      }
      fs::path orig{info.path};
      if (orig.is_relative()) {
        if (loc.topdir.empty()) {
          continue;  // the home trash records absolute paths
        }
        orig = loc.topdir / orig;
      }
      e.original_path = orig.lexically_normal();
      e.deletion_date = info.date;
      out.push_back(std::move(e));
    }
  }
  return out;
}

OpResult restore_trash_entry(const TrashEntry& entry, const Options& options)
{
  if (cancelled(options)) {
    return Result{.items = {}, .cancelled = true};
  }
  if (!safe_entry_name(entry.name)) {
    return std::unexpected(errno_error(EINVAL, entry.stored_path(), "invalid trash entry name"));
  }
  const fs::path dest = entry.original_path;
  if (dest.empty() || dest.is_relative()) {
    return std::unexpected(errno_error(EINVAL, entry.info_path(), "trash entry has no usable Path"));
  }
  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = entry.stored_path(), .destination = dest});
    return r;
  }
  struct stat st {};
  if (lstat_path(dest, &st)) {
    return std::unexpected(errno_error(EEXIST, dest, "cannot restore: the original path exists"));
  }
  std::error_code ec;
  fs::create_directories(dest.parent_path(), ec);
  if (ec) {
    return std::unexpected(Error{ec, dest.parent_path(), "cannot create the original folder"});
  }
  if (rename_noreplace(entry.stored_path().c_str(), dest.c_str()) != 0) {
    return std::unexpected(errno_error(errno, dest, "cannot restore the item"));
  }
  ::unlink(entry.info_path().c_str());
  Result r;
  r.items.push_back(ItemResult{.source = entry.stored_path(), .destination = dest});
  return r;
}

OpResult delete_trash_entry(const TrashEntry& entry, const Options& options)
{
  if (cancelled(options)) {
    return Result{.items = {}, .cancelled = true};
  }
  if (!safe_entry_name(entry.name)) {
    return std::unexpected(errno_error(EINVAL, entry.stored_path(), "invalid trash entry name"));
  }
  if (!options.dry_run) {
    std::error_code ec;
    fs::remove_all(entry.stored_path(), ec);
    if (ec) {
      return std::unexpected(Error{ec, entry.stored_path(), "cannot delete the trashed item"});
    }
    fs::remove(entry.info_path(), ec);
    if (ec) {
      return std::unexpected(Error{ec, entry.info_path(), "cannot delete the info file"});
    }
  }
  Result r;
  r.items.push_back(ItemResult{.source = entry.stored_path(), .destination = {}});
  return r;
}

OpResult empty_trash(const TrashOptions& opts, const Options& options)
{
  Result r;
  for (const auto& loc : all_trash_dirs(opts)) {
    for (const char* sub : {"files", "info"}) {
      std::error_code ec;
      fs::directory_iterator it(loc.dir / sub, ec);
      // Collect first: removing while iterating invalidates the iterator.
      std::vector<fs::path> victims;
      for (; !ec && it != fs::directory_iterator{}; it.increment(ec)) {
        victims.push_back(it->path());
      }
      for (const auto& victim : victims) {
        if (cancelled(options)) {
          r.cancelled = true;
          return r;
        }
        if (options.dry_run) {
          continue;
        }
        std::error_code rec;
        fs::remove_all(victim, rec);
        if (rec) {
          return std::unexpected(Error{rec, victim, "cannot delete from the trash"});
        }
      }
    }
    if (!options.dry_run) {
      std::error_code ec;
      fs::remove(loc.dir / "directorysizes", ec);
    }
    r.items.push_back(ItemResult{.source = loc.dir, .destination = {}});
  }
  return r;
}

} // namespace dirops

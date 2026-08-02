// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/archive/archive_index.hpp"

#include <string>

#if defined(DIRTOO_HAS_LIBARCHIVE)

#include <archive.h>
#include <archive_entry.h>

#include <filesystem>
#include <vector>

namespace dirtoo::archive {
namespace {

struct ArchiveReadGuard {
  archive* a = nullptr;
  ~ArchiveReadGuard()
  {
    if (a != nullptr) {
      archive_read_free(a);
    }
  }
};

struct ArchiveWriteGuard {
  archive* a = nullptr;
  ~ArchiveWriteGuard()
  {
    if (a != nullptr) {
      archive_write_free(a);
    }
  }
};

int copy_data(archive* ar, archive* aw)
{
  const void* buff = nullptr;
  size_t size = 0;
  la_int64_t offset = 0;
  for (;;) {
    const int r = archive_read_data_block(ar, &buff, &size, &offset);
    if (r == ARCHIVE_EOF) {
      return ARCHIVE_OK;
    }
    if (r < ARCHIVE_OK) {
      return r;
    }
    const la_ssize_t written = archive_write_data_block(aw, buff, size, offset);
    if (written < 0) {
      return static_cast<int>(written);
    }
  }
}

} // namespace

bool libarchive_available()
{
  return true;
}

std::expected<std::vector<ArchiveEntry>, std::string>
list_archive_entries_libarchive(const std::filesystem::path& archive_file)
{
  ArchiveReadGuard guard;
  guard.a = archive_read_new();
  if (guard.a == nullptr) {
    return std::unexpected("archive_read_new failed");
  }
  archive_read_support_filter_all(guard.a);
  archive_read_support_format_all(guard.a);

  if (archive_read_open_filename(guard.a, archive_file.string().c_str(), 10240) != ARCHIVE_OK) {
    return std::unexpected(archive_error_string(guard.a)
                               ? archive_error_string(guard.a)
                               : "archive_read_open_filename failed");
  }

  std::vector<ArchiveEntry> out;
  archive_entry* entry = nullptr;
  while (archive_read_next_header(guard.a, &entry) == ARCHIVE_OK) {
    const char* pathname = archive_entry_pathname(entry);
    if (pathname == nullptr || pathname[0] == '\0') {
      archive_read_data_skip(guard.a);
      continue;
    }
    std::string name = pathname;
    if (name.starts_with("./")) {
      name.erase(0, 2);
    }
    const bool is_dir = archive_entry_filetype(entry) == AE_IFDIR
                        || (!name.empty() && name.back() == '/');
    if (!name.empty() && name.back() == '/') {
      name.pop_back();
    }
    if (name.empty()) {
      archive_read_data_skip(guard.a);
      continue;
    }
    ArchiveEntry e;
    e.path = std::filesystem::path{name}.lexically_normal();
    e.is_directory = is_dir;
    const la_int64_t sz = archive_entry_size(entry);
    e.size = sz > 0 ? static_cast<std::uint64_t>(sz) : 0;
    out.push_back(std::move(e));
    archive_read_data_skip(guard.a);
  }
  return out;
}

std::expected<void, std::string>
extract_archive_libarchive(const std::filesystem::path& archive_file,
                           const std::filesystem::path& dest_dir)
{
  std::error_code ec;
  std::filesystem::create_directories(dest_dir, ec);
  if (ec) {
    return std::unexpected(ec.message());
  }

  ArchiveReadGuard rin;
  rin.a = archive_read_new();
  if (rin.a == nullptr) {
    return std::unexpected("archive_read_new failed");
  }
  archive_read_support_filter_all(rin.a);
  archive_read_support_format_all(rin.a);
  if (archive_read_open_filename(rin.a, archive_file.string().c_str(), 10240) != ARCHIVE_OK) {
    return std::unexpected(archive_error_string(rin.a)
                               ? archive_error_string(rin.a)
                               : "archive_read_open_filename failed");
  }

  ArchiveWriteGuard wout;
  wout.a = archive_write_disk_new();
  if (wout.a == nullptr) {
    return std::unexpected("archive_write_disk_new failed");
  }
  const int flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_SECURE_NODOTDOT
                    | ARCHIVE_EXTRACT_SECURE_SYMLINKS;
  archive_write_disk_set_options(wout.a, flags);
  archive_write_disk_set_standard_lookup(wout.a);

  archive_entry* entry = nullptr;
  while (archive_read_next_header(rin.a, &entry) == ARCHIVE_OK) {
    const char* pathname = archive_entry_pathname(entry);
    if (pathname == nullptr) {
      archive_read_data_skip(rin.a);
      continue;
    }
    // Extract under dest_dir
    const auto full = dest_dir / pathname;
    archive_entry_set_pathname(entry, full.string().c_str());

    if (archive_write_header(wout.a, entry) != ARCHIVE_OK) {
      return std::unexpected(archive_error_string(wout.a)
                                 ? archive_error_string(wout.a)
                                 : "archive_write_header failed");
    }
    if (archive_entry_size(entry) > 0) {
      if (copy_data(rin.a, wout.a) < ARCHIVE_OK) {
        return std::unexpected(archive_error_string(rin.a)
                                   ? archive_error_string(rin.a)
                                   : "archive extract data failed");
      }
    }
    archive_write_finish_entry(wout.a);
  }
  return {};
}

std::expected<std::filesystem::path, std::string>
extract_member_libarchive(const std::filesystem::path& archive_file,
                          const std::filesystem::path& member,
                          const std::filesystem::path& dest_dir)
{
  std::error_code ec;
  std::filesystem::create_directories(dest_dir, ec);
  if (ec) {
    return std::unexpected(ec.message());
  }

  const std::string want = member.generic_string();

  ArchiveReadGuard rin;
  rin.a = archive_read_new();
  if (rin.a == nullptr) {
    return std::unexpected("archive_read_new failed");
  }
  archive_read_support_filter_all(rin.a);
  archive_read_support_format_all(rin.a);
  if (archive_read_open_filename(rin.a, archive_file.string().c_str(), 10240) != ARCHIVE_OK) {
    return std::unexpected(archive_error_string(rin.a)
                               ? archive_error_string(rin.a)
                               : "archive_read_open_filename failed");
  }

  ArchiveWriteGuard wout;
  wout.a = archive_write_disk_new();
  if (wout.a == nullptr) {
    return std::unexpected("archive_write_disk_new failed");
  }
  archive_write_disk_set_options(wout.a, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM
                                                 | ARCHIVE_EXTRACT_SECURE_NODOTDOT);
  archive_write_disk_set_standard_lookup(wout.a);

  archive_entry* entry = nullptr;
  bool found = false;
  while (archive_read_next_header(rin.a, &entry) == ARCHIVE_OK) {
    const char* pathname = archive_entry_pathname(entry);
    if (pathname == nullptr) {
      archive_read_data_skip(rin.a);
      continue;
    }
    std::string name = pathname;
    if (name.starts_with("./")) {
      name.erase(0, 2);
    }
    if (!name.empty() && name.back() == '/') {
      name.pop_back();
    }
    if (name != want) {
      archive_read_data_skip(rin.a);
      continue;
    }
    found = true;
    const auto full = dest_dir / member;
    if (auto parent = full.parent_path(); !parent.empty()) {
      std::filesystem::create_directories(parent, ec);
    }
    archive_entry_set_pathname(entry, full.string().c_str());
    if (archive_write_header(wout.a, entry) != ARCHIVE_OK) {
      return std::unexpected(archive_error_string(wout.a)
                                 ? archive_error_string(wout.a)
                                 : "archive_write_header failed");
    }
    if (archive_entry_size(entry) > 0) {
      if (copy_data(rin.a, wout.a) < ARCHIVE_OK) {
        return std::unexpected(archive_error_string(rin.a)
                                   ? archive_error_string(rin.a)
                                   : "archive extract data failed");
      }
    }
    archive_write_finish_entry(wout.a);
    break;
  }
  if (!found) {
    return std::unexpected("member not found in archive: " + want);
  }
  const auto dest = dest_dir / member;
  if (!std::filesystem::exists(dest)) {
    return std::unexpected("extracted member not found at " + dest.string());
  }
  return dest;
}

} // namespace dirtoo::archive

#else // !DIRTOO_HAS_LIBARCHIVE

namespace dirtoo::archive {

bool libarchive_available()
{
  return false;
}

std::expected<std::vector<ArchiveEntry>, std::string>
list_archive_entries_libarchive(const std::filesystem::path&)
{
  return std::unexpected("libarchive support not enabled at build time");
}

std::expected<void, std::string>
extract_archive_libarchive(const std::filesystem::path&, const std::filesystem::path&)
{
  return std::unexpected("libarchive support not enabled at build time");
}

std::expected<std::filesystem::path, std::string>
extract_member_libarchive(const std::filesystem::path&, const std::filesystem::path&,
                          const std::filesystem::path&)
{
  return std::unexpected("libarchive support not enabled at build time");
}

} // namespace dirtoo::archive

#endif

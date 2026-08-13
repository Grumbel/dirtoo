// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/fs/file_info.hpp"

#include <system_error>

#if !defined(_WIN32)
#  include <sys/stat.h>
#  include <sys/types.h>
#endif

namespace dirtoo::fs {
namespace {

/// Directory metadata size (st_size) and file sizes. std::filesystem::file_size
/// is only defined for regular files and fails on directories, which left every
/// folder at 0 B despite ls/stat reporting a non-zero inode size.
[[nodiscard]] std::uint64_t read_size_bytes(const std::filesystem::path& path, bool is_directory,
                                            bool is_regular_file)
{
  if (!is_directory && !is_regular_file) {
    return 0;
  }
#if !defined(_WIN32)
  struct stat st {};
  // lstat: size of the symlink itself when applicable; for dirs/files matches ls -l.
  if (::lstat(path.c_str(), &st) == 0) {
    return static_cast<std::uint64_t>(st.st_size);
  }
  return 0;
#else
  std::error_code ec;
  const auto sz = std::filesystem::file_size(path, ec);
  if (!ec) {
    return static_cast<std::uint64_t>(sz);
  }
  return 0;
#endif
}

} // namespace

void FileInfo::fill_posix_times_from_path(const std::filesystem::path& path)
{
#if !defined(_WIN32)
  struct stat st {};
  if (::lstat(path.c_str(), &st) != 0) {
    return;
  }
  const auto to_sys = [](const struct timespec& ts) {
    return std::chrono::system_clock::time_point{
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec))};
  };
#  if defined(st_atim) || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) \
      || defined(__NetBSD__) || defined(__OpenBSD__)
  atime_ = to_sys(st.st_atim);
  has_atime_ = true;
  ctime_ = to_sys(st.st_ctim);
  has_ctime_ = true;
#  else
  atime_ = std::chrono::system_clock::from_time_t(st.st_atime);
  has_atime_ = true;
  ctime_ = std::chrono::system_clock::from_time_t(st.st_ctime);
  has_ctime_ = true;
#  endif
#  if defined(__APPLE__)
  birthtime_ = to_sys(st.st_birthtimespec);
  has_birthtime_ = st.st_birthtimespec.tv_sec != 0;
#  elif defined(__FreeBSD__)
  birthtime_ = to_sys(st.st_birthtim);
  has_birthtime_ = st.st_birthtim.tv_sec != 0;
#  endif
#else
  (void)path;
#endif
}

FileInfo FileInfo::from_path(const std::filesystem::path& path)
{
  FileInfo info;
  info.path_ = path;
  info.location_ = Location::from_path(path);
  info.display_name_ = path.filename().string();

  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec) {
    return info;
  }

  info.is_symlink_ = std::filesystem::is_symlink(status);
  info.is_directory_ = std::filesystem::is_directory(status);
  info.is_regular_file_ = std::filesystem::is_regular_file(status);
  info.permissions_ = status.permissions();
  info.size_ = read_size_bytes(path, info.is_directory_, info.is_regular_file_);
  info.mtime_ = std::filesystem::last_write_time(path, ec);
  info.fill_posix_times_from_path(path);
  return info;
}

FileInfo FileInfo::from_location(const Location& location)
{
  if (location.is_archive()) {
    return synthetic(location, location.basename(), false, 0);
  }
  return from_path(location.as_path());
}

FileInfo FileInfo::synthetic(Location location, std::string display_name, bool is_directory,
                             std::uint64_t size)
{
  FileInfo info;
  info.location_ = std::move(location);
  info.display_name_ = std::move(display_name);
  // Archive members all share the container path via as_path(); use the full
  // location URL so model keys (thumbnails, child counts, PathRole) stay unique.
  if (info.location_.is_archive()) {
    info.path_ = std::filesystem::path{info.location_.as_url()};
  } else {
    info.path_ = info.location_.as_path();
  }
  info.size_ = size;
  info.is_directory_ = is_directory;
  info.is_regular_file_ = !is_directory;
  info.is_synthetic_ = true;
  return info;
}

void FileInfo::set_mtime_unix(std::int64_t sec)
{
  if (sec <= 0) {
    return;
  }
  try {
    const auto sys = std::chrono::system_clock::time_point{std::chrono::seconds{sec}};
    mtime_ = std::chrono::clock_cast<std::filesystem::file_time_type::clock>(sys);
  } catch (...) {
    // Leave default mtime on conversion failure.
  }
}

std::string FileInfo::basename() const
{
  if (!display_name_.empty()) {
    return display_name_;
  }
  if (location_.is_archive() && !location_.entry_path().empty()) {
    return location_.entry_path().filename().string();
  }
  return path_.filename().string();
}

std::string FileInfo::extension() const
{
  const auto name = basename();
  const auto pos = name.rfind('.');
  if (pos == std::string::npos || pos == 0) {
    return {};
  }
  return name.substr(pos);
}

FileInfo FileInfo::from_directory_entry(const std::filesystem::directory_entry& entry)
{
  FileInfo info;
  info.path_ = entry.path();
  // Parent was already normalized when the user navigated; avoid weakly_canonical
  // (extra stats) on every child during large listings.
  info.location_ = Location::from_path_unchecked(entry.path());
  info.display_name_ = entry.path().filename().string();

  std::error_code ec;
  const auto status = entry.symlink_status(ec);
  if (ec) {
    return info;
  }

  info.is_symlink_ = std::filesystem::is_symlink(status);
  info.is_directory_ = std::filesystem::is_directory(status);
  info.is_regular_file_ = std::filesystem::is_regular_file(status);
  info.permissions_ = status.permissions();
  info.size_ = read_size_bytes(entry.path(), info.is_directory_, info.is_regular_file_);
  info.mtime_ = entry.last_write_time(ec);
  info.fill_posix_times_from_path(entry.path());
  return info;
}

std::vector<FileInfo> list_directory(const Location& location)
{
  std::vector<FileInfo> result;
  if (location.is_archive()) {
    return result;
  }
  std::error_code ec;
  const auto opts = std::filesystem::directory_options::skip_permission_denied;
  for (const auto& entry : std::filesystem::directory_iterator(location.as_path(), opts, ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    result.push_back(FileInfo::from_directory_entry(entry));
  }
  return result;
}

} // namespace dirtoo::fs

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/fs/file_info.hpp"

#include <chrono>
#include <cstdint>
#include <system_error>

#if !defined(_WIN32)
#  include <sys/stat.h>
#  include <sys/types.h>
#endif

namespace dirtoo::fs {
namespace {

#if !defined(_WIN32)
[[nodiscard]] std::chrono::system_clock::time_point timespec_to_sys(const struct timespec& ts)
{
  using namespace std::chrono;
  auto tp = system_clock::time_point{seconds{ts.tv_sec}};
  if (ts.tv_nsec > 0) {
    tp += duration_cast<system_clock::duration>(nanoseconds{ts.tv_nsec});
  }
  return tp;
}
#endif

} // namespace

#if !defined(_WIN32)
void FileInfo::apply_posix_stat(const struct stat& st)
{
  is_symlink_ = S_ISLNK(st.st_mode);
  is_directory_ = S_ISDIR(st.st_mode);
  is_regular_file_ = S_ISREG(st.st_mode);
  size_ = static_cast<std::uint64_t>(st.st_size >= 0 ? st.st_size : 0);
  permissions_ = static_cast<std::filesystem::perms>(st.st_mode & 07777);

#  if defined(__APPLE__)
  atime_ = timespec_to_sys(st.st_atimespec);
  ctime_ = timespec_to_sys(st.st_ctimespec);
  has_atime_ = true;
  has_ctime_ = true;
  birthtime_ = timespec_to_sys(st.st_birthtimespec);
  has_birthtime_ = st.st_birthtimespec.tv_sec != 0;
  try {
    const auto sys = timespec_to_sys(st.st_mtimespec);
    mtime_ = std::chrono::clock_cast<std::filesystem::file_time_type::clock>(sys);
  } catch (...) {
  }
#  elif defined(__FreeBSD__)
  atime_ = timespec_to_sys(st.st_atim);
  ctime_ = timespec_to_sys(st.st_ctim);
  has_atime_ = true;
  has_ctime_ = true;
  birthtime_ = timespec_to_sys(st.st_birthtim);
  has_birthtime_ = st.st_birthtim.tv_sec != 0;
  try {
    const auto sys = timespec_to_sys(st.st_mtim);
    mtime_ = std::chrono::clock_cast<std::filesystem::file_time_type::clock>(sys);
  } catch (...) {
  }
#  else
  atime_ = timespec_to_sys(st.st_atim);
  ctime_ = timespec_to_sys(st.st_ctim);
  has_atime_ = true;
  has_ctime_ = true;
  try {
    const auto sys = timespec_to_sys(st.st_mtim);
    mtime_ = std::chrono::clock_cast<std::filesystem::file_time_type::clock>(sys);
  } catch (...) {
  }
#  endif
}
#endif

void FileInfo::fill_posix_times_from_path(const std::filesystem::path& path)
{
#if !defined(_WIN32)
  struct stat st {};
  if (::lstat(path.c_str(), &st) != 0) {
    return;
  }
#  if defined(__APPLE__)
  atime_ = timespec_to_sys(st.st_atimespec);
  ctime_ = timespec_to_sys(st.st_ctimespec);
  has_atime_ = true;
  has_ctime_ = true;
  birthtime_ = timespec_to_sys(st.st_birthtimespec);
  has_birthtime_ = st.st_birthtimespec.tv_sec != 0;
#  elif defined(__FreeBSD__)
  atime_ = timespec_to_sys(st.st_atim);
  ctime_ = timespec_to_sys(st.st_ctim);
  has_atime_ = true;
  has_ctime_ = true;
  birthtime_ = timespec_to_sys(st.st_birthtim);
  has_birthtime_ = st.st_birthtim.tv_sec != 0;
#  else
  atime_ = timespec_to_sys(st.st_atim);
  ctime_ = timespec_to_sys(st.st_ctim);
  has_atime_ = true;
  has_ctime_ = true;
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

#if !defined(_WIN32)
  struct stat st {};
  if (::lstat(path.c_str(), &st) == 0) {
    info.apply_posix_stat(st);
    return info;
  }
#endif

  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec) {
    return info;
  }

  info.is_symlink_ = std::filesystem::is_symlink(status);
  info.is_directory_ = std::filesystem::is_directory(status);
  info.is_regular_file_ = std::filesystem::is_regular_file(status);
  info.permissions_ = status.permissions();
  if (info.is_directory_ || info.is_regular_file_) {
    const auto sz = std::filesystem::file_size(path, ec);
    if (!ec) {
      info.size_ = static_cast<std::uint64_t>(sz);
    }
  }
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
  const std::filesystem::path path = entry.path();
  info.path_ = path;
  // Parent was already normalized when the user navigated; avoid weakly_canonical
  // (extra stats) on every child during large listings.
  info.location_ = Location::from_path_unchecked(path);
  info.display_name_ = path.filename().string();

#if !defined(_WIN32)
  // Single lstat for type/size/times — fewer stack frames than the previous
  // read_size_bytes + last_write_time + fill_posix_times_from_path sequence.
  struct stat st {};
  if (::lstat(path.c_str(), &st) == 0) {
    info.apply_posix_stat(st);
    return info;
  }
#endif

  std::error_code ec;
  const auto status = entry.symlink_status(ec);
  if (ec) {
    return info;
  }

  info.is_symlink_ = std::filesystem::is_symlink(status);
  info.is_directory_ = std::filesystem::is_directory(status);
  info.is_regular_file_ = std::filesystem::is_regular_file(status);
  info.permissions_ = status.permissions();
  if (info.is_directory_ || info.is_regular_file_) {
    const auto sz = std::filesystem::file_size(path, ec);
    if (!ec) {
      info.size_ = static_cast<std::uint64_t>(sz);
    }
  }
  info.mtime_ = entry.last_write_time(ec);
  info.fill_posix_times_from_path(path);
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

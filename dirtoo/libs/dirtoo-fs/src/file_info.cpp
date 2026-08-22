// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/fs/file_info.hpp"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <system_error>

#if !defined(_WIN32)
#  include <sys/stat.h>
#  include <sys/types.h>
#endif

namespace dirtoo::fs {
namespace {

[[nodiscard]] std::chrono::system_clock::time_point sys_from_unix(std::time_t sec)
{
  // Whole seconds only — avoids timespec/nsec edge cases and mixed-duration math.
  return std::chrono::system_clock::from_time_t(sec);
}

} // namespace

#if !defined(_WIN32)
void FileInfo::apply_posix_stat(const struct stat& st)
{
  is_symlink_ = S_ISLNK(st.st_mode) != 0;
  is_directory_ = S_ISDIR(st.st_mode) != 0;
  is_regular_file_ = S_ISREG(st.st_mode) != 0;
  size_ = static_cast<std::uint64_t>(st.st_size >= 0 ? st.st_size : 0);
  permissions_ = static_cast<std::filesystem::perms>(st.st_mode & 07777);

  // Prefer st_mtim.tv_sec when available; fall back to st_mtime.
#  if defined(__APPLE__)
  const std::time_t msec = st.st_mtimespec.tv_sec;
  const std::time_t asec = st.st_atimespec.tv_sec;
  const std::time_t csec = st.st_ctimespec.tv_sec;
  const std::time_t bsec = st.st_birthtimespec.tv_sec;
  has_birthtime_ = bsec != 0;
  if (has_birthtime_) {
    birthtime_ = sys_from_unix(bsec);
  }
#  elif defined(__FreeBSD__)
  const std::time_t msec = st.st_mtim.tv_sec;
  const std::time_t asec = st.st_atim.tv_sec;
  const std::time_t csec = st.st_ctim.tv_sec;
  const std::time_t bsec = st.st_birthtim.tv_sec;
  has_birthtime_ = bsec != 0;
  if (has_birthtime_) {
    birthtime_ = sys_from_unix(bsec);
  }
#  else
  // Linux / generic POSIX.1-2008
  const std::time_t msec = st.st_mtim.tv_sec;
  const std::time_t asec = st.st_atim.tv_sec;
  const std::time_t csec = st.st_ctim.tv_sec;
#  endif

  set_mtime_unix(static_cast<std::int64_t>(msec));
  atime_ = sys_from_unix(asec);
  ctime_ = sys_from_unix(csec);
  has_atime_ = true;
  has_ctime_ = true;
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
  atime_ = sys_from_unix(st.st_atimespec.tv_sec);
  ctime_ = sys_from_unix(st.st_ctimespec.tv_sec);
  has_atime_ = true;
  has_ctime_ = true;
  if (st.st_birthtimespec.tv_sec != 0) {
    birthtime_ = sys_from_unix(st.st_birthtimespec.tv_sec);
    has_birthtime_ = true;
  }
#  elif defined(__FreeBSD__)
  atime_ = sys_from_unix(st.st_atim.tv_sec);
  ctime_ = sys_from_unix(st.st_ctim.tv_sec);
  has_atime_ = true;
  has_ctime_ = true;
  if (st.st_birthtim.tv_sec != 0) {
    birthtime_ = sys_from_unix(st.st_birthtim.tv_sec);
    has_birthtime_ = true;
  }
#  else
  atime_ = sys_from_unix(st.st_atim.tv_sec);
  ctime_ = sys_from_unix(st.st_ctim.tv_sec);
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
  {
    const auto ft = std::filesystem::last_write_time(path, ec);
    if (!ec) {
      info.mtime_ = ft;
    }
  }
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
    const auto sys = std::chrono::system_clock::from_time_t(static_cast<std::time_t>(sec));
#if defined(__cpp_lib_chrono) && (__cpp_lib_chrono >= 201907L)
    // C++20: file_clock ↔ system_clock
    mtime_ = std::chrono::file_clock::from_sys(sys);
#else
    mtime_ = std::chrono::clock_cast<std::filesystem::file_time_type::clock>(sys);
#endif
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
  // Copy path once; avoid re-entering directory_entry APIs for metadata.
  const std::filesystem::path path = entry.path();
  info.path_ = path;
  info.location_ = Location::from_path_unchecked(path);
  try {
    info.display_name_ = path.filename().string();
  } catch (...) {
    info.display_name_.clear();
  }

#if !defined(_WIN32)
  struct stat st {};
  if (::lstat(path.c_str(), &st) == 0) {
    info.apply_posix_stat(st);
    return info;
  }
#endif

  // Fallback when lstat fails (e.g. vanished entry): best-effort via std::filesystem.
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
  {
    const auto ft = entry.last_write_time(ec);
    if (!ec) {
      info.mtime_ = ft;
    }
  }
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

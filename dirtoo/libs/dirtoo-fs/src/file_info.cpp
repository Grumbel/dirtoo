// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/fs/file_info.hpp"

#include <system_error>

namespace dirtoo::fs {

FileInfo FileInfo::from_path(const std::filesystem::path& path)
{
  FileInfo info;
  info.path_ = path;
  info.location_ = Location::from_path(path);

  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec) {
    return info;
  }

  info.is_symlink_ = std::filesystem::is_symlink(status);
  info.is_directory_ = std::filesystem::is_directory(status);
  info.is_regular_file_ = std::filesystem::is_regular_file(status);
  info.permissions_ = status.permissions();

  if (info.is_regular_file_) {
    info.size_ = static_cast<std::uint64_t>(std::filesystem::file_size(path, ec));
  }

  info.mtime_ = std::filesystem::last_write_time(path, ec);
  return info;
}

FileInfo FileInfo::from_location(const Location& location)
{
  return from_path(location.as_path());
}

FileInfo FileInfo::synthetic(Location location, std::string basename, bool is_directory,
                             std::uint64_t size)
{
  FileInfo info;
  info.location_ = std::move(location);
  info.path_ = info.location_.as_path() / basename;
  if (info.location_.is_archive()) {
    // path() remains archive file path for ops; basename comes from location entry.
    info.path_ = info.location_.as_path();
  }
  info.size_ = size;
  info.is_directory_ = is_directory;
  info.is_regular_file_ = !is_directory;
  info.is_synthetic_ = true;
  return info;
}


std::string FileInfo::basename() const
{
  if (location_.is_archive() && !location_.entry_path().empty()) {
    return location_.entry_path().filename().string();
  }
  if (is_synthetic_ && location_.is_archive()) {
    return location_.basename();
  }
  return path_.filename().string();
}

std::string FileInfo::extension() const
{
  return path_.extension().string();
}

std::vector<FileInfo> list_directory(const Location& location)
{
  std::vector<FileInfo> result;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(location.as_path(), ec)) {
    if (ec) {
      break;
    }
    result.push_back(FileInfo::from_path(entry.path()));
  }
  return result;
}

} // namespace dirtoo::fs

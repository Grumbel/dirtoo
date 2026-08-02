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

  // Regular files and directories both expose st_size (dir metadata size on Linux).
  if (info.is_regular_file_ || info.is_directory_) {
    const auto sz = std::filesystem::file_size(path, ec);
    if (!ec) {
      info.size_ = static_cast<std::uint64_t>(sz);
    }
  }

  info.mtime_ = std::filesystem::last_write_time(path, ec);
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

  // Regular files and directories both expose st_size (dir metadata size on Linux).
  if (info.is_regular_file_ || info.is_directory_) {
    info.size_ = static_cast<std::uint64_t>(entry.file_size(ec));
    if (ec) {
      info.size_ = 0;
    }
  }

  info.mtime_ = entry.last_write_time(ec);
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

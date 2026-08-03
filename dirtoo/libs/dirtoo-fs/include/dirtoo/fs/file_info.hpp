// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dirtoo::fs {

class FileInfo {
public:
  [[nodiscard]] static FileInfo from_path(const std::filesystem::path& path);
  [[nodiscard]] static FileInfo from_location(const Location& location);
  /// Build from a directory_iterator entry using cached status (listing hot path).
  [[nodiscard]] static FileInfo from_directory_entry(const std::filesystem::directory_entry& entry);

  /// Virtual entry (e.g. archive member) that may not exist on the real FS.
  [[nodiscard]] static FileInfo synthetic(Location location, std::string display_name,
                                          bool is_directory, std::uint64_t size = 0);

  [[nodiscard]] const Location& location() const noexcept { return location_; }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

  [[nodiscard]] std::string basename() const;
  [[nodiscard]] std::string extension() const;

  [[nodiscard]] std::uint64_t size() const noexcept { return size_; }
  [[nodiscard]] std::filesystem::file_time_type mtime() const noexcept { return mtime_; }

  /// POSIX atime / ctime / birth (creation) as system_clock time points.
  /// Zero epoch means "unknown / not available" (synthetic entries, failed stat).
  [[nodiscard]] std::chrono::system_clock::time_point atime() const noexcept { return atime_; }
  [[nodiscard]] std::chrono::system_clock::time_point ctime() const noexcept { return ctime_; }
  [[nodiscard]] std::chrono::system_clock::time_point birthtime() const noexcept { return birthtime_; }
  [[nodiscard]] bool has_atime() const noexcept { return has_atime_; }
  [[nodiscard]] bool has_ctime() const noexcept { return has_ctime_; }
  [[nodiscard]] bool has_birthtime() const noexcept { return has_birthtime_; }

  [[nodiscard]] bool is_directory() const noexcept { return is_directory_; }
  [[nodiscard]] bool is_regular_file() const noexcept { return is_regular_file_; }
  [[nodiscard]] bool is_symlink() const noexcept { return is_symlink_; }
  [[nodiscard]] bool is_synthetic() const noexcept { return is_synthetic_; }

  [[nodiscard]] std::filesystem::perms permissions() const noexcept { return permissions_; }

private:
  void fill_posix_times_from_path(const std::filesystem::path& path);

  Location location_;
  std::filesystem::path path_;
  std::string display_name_;
  std::uint64_t size_ = 0;
  std::filesystem::file_time_type mtime_{};
  std::chrono::system_clock::time_point atime_{};
  std::chrono::system_clock::time_point ctime_{};
  std::chrono::system_clock::time_point birthtime_{};
  bool has_atime_ = false;
  bool has_ctime_ = false;
  bool has_birthtime_ = false;
  bool is_directory_ = false;
  bool is_regular_file_ = false;
  bool is_symlink_ = false;
  bool is_synthetic_ = false;
  std::filesystem::perms permissions_{};
};

/// List non-recursive directory entries. Hidden files included; caller filters.
[[nodiscard]] std::vector<FileInfo> list_directory(const Location& location);

} // namespace dirtoo::fs

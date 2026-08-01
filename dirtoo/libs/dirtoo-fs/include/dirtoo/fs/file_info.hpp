// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dirtoo::fs {

class FileInfo {
public:
  [[nodiscard]] static FileInfo from_path(const std::filesystem::path& path);
  [[nodiscard]] static FileInfo from_location(const Location& location);

  [[nodiscard]] const Location& location() const noexcept { return location_; }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

  [[nodiscard]] std::string basename() const;
  [[nodiscard]] std::string extension() const;

  [[nodiscard]] std::uint64_t size() const noexcept { return size_; }
  [[nodiscard]] std::filesystem::file_time_type mtime() const noexcept { return mtime_; }

  [[nodiscard]] bool is_directory() const noexcept { return is_directory_; }
  [[nodiscard]] bool is_regular_file() const noexcept { return is_regular_file_; }
  [[nodiscard]] bool is_symlink() const noexcept { return is_symlink_; }

  [[nodiscard]] std::filesystem::perms permissions() const noexcept { return permissions_; }

private:
  Location location_;
  std::filesystem::path path_;
  std::uint64_t size_ = 0;
  std::filesystem::file_time_type mtime_{};
  bool is_directory_ = false;
  bool is_regular_file_ = false;
  bool is_symlink_ = false;
  std::filesystem::perms permissions_{};
};

/// List non-recursive directory entries. Hidden files included; caller filters.
[[nodiscard]] std::vector<FileInfo> list_directory(const Location& location);

} // namespace dirtoo::fs

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace dirtoo::app {

/// Persistent bookmarks file (one location URL per line), same idea as
/// Python `bookmark/bookmarks.py`.
class Bookmarks {
public:
  explicit Bookmarks(std::filesystem::path file);

  [[nodiscard]] static std::filesystem::path default_path();

  [[nodiscard]] std::vector<fs::Location> entries() const;
  [[nodiscard]] bool contains(const fs::Location& location) const;
  void append(const fs::Location& location);
  void remove(const fs::Location& location);
  [[nodiscard]] bool toggle(const fs::Location& location); // true if now bookmarked

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
  void write_all(const std::vector<fs::Location>& entries) const;

  std::filesystem::path path_;
};

} // namespace dirtoo::app

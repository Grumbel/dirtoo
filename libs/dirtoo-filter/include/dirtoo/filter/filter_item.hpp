// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace dirtoo::filter {

/// Minimal item description for matching (no Qt, no dirtoo-fs dependency).
struct FilterItem {
  std::string name;
  std::uint64_t size = 0;
  bool is_directory = false;
  std::filesystem::path path;
  /// Unix epoch seconds when known (search / collection fill this).
  std::optional<std::int64_t> mtime_sec;
};

} // namespace dirtoo::filter

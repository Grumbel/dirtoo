// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

namespace dirtoo::hash {

/// What identifies "this version of the file" for cache validity: size and
/// modification time. The time is **nanoseconds since the Unix epoch**
/// (st_mtim), never `file_time_type::time_since_epoch()`: that count is
/// relative to an implementation-defined epoch and unit.
struct FileStamp {
  std::uint64_t size = 0;
  std::optional<std::int64_t> mtime_ns;

  [[nodiscard]] bool operator==(const FileStamp&) const = default;
};

/// stat() the file (following symlinks, like file_size()). nullopt on error.
[[nodiscard]] std::optional<FileStamp> stat_stamp(const std::filesystem::path& path);

/// Convert a std::filesystem time to Unix nanoseconds.
[[nodiscard]] std::int64_t unix_ns_from_file_time(std::filesystem::file_time_type t);

/// Offset to add to a legacy `file_time_type::time_since_epoch().count()`
/// (as stored by dirtoo before schema version 1) to get Unix nanoseconds.
[[nodiscard]] std::int64_t legacy_file_clock_offset_ns();

} // namespace dirtoo::hash

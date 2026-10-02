// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMetaType>

#include <cstdint>
#include <filesystem>
#include <optional>

namespace dirtoo::app {

/// What the conflict dialog shows about one side of a name clash.
struct ConflictEntryInfo {
  bool exists = false;
  bool is_directory = false;
  std::optional<std::uint64_t> size;
  std::optional<std::filesystem::file_time_type> mtime;
};

/// Source/destination metadata for ask_conflict_policy(). Gathered on a worker
/// thread so the dialog itself never stats paths on (possibly slow) drives.
struct ConflictProbe {
  ConflictEntryInfo source;
  ConflictEntryInfo destination;
};

/// Blocking stat of both paths (empty paths are left as "does not exist").
/// Worker threads only.
[[nodiscard]] ConflictProbe probe_conflict(const std::filesystem::path& source,
                                           const std::filesystem::path& destination);

} // namespace dirtoo::app

Q_DECLARE_METATYPE(dirtoo::app::ConflictProbe)

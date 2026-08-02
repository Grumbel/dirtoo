// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirops/ops.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace dtcli {

inline bool is_version_flag(std::string_view a)
{
  return a == "--version" || a == "-V";
}

inline void print_version()
{
  std::cout << "dirtoo " DIRTOO_VERSION "\n";
}

inline dirops::ConflictPolicy parse_conflict(std::string_view s)
{
  if (s == "overwrite" || s == "always" || s == "Y") {
    return dirops::ConflictPolicy::Overwrite;
  }
  if (s == "rename") {
    return dirops::ConflictPolicy::Rename;
  }
  if (s == "skip" || s == "never" || s == "N") {
    return dirops::ConflictPolicy::Skip;
  }
  return dirops::ConflictPolicy::Fail;
}

/// Destination path for one source when using --target-directory.
inline std::filesystem::path dest_under_target(const std::filesystem::path& source,
                                               const std::filesystem::path& target_dir,
                                               bool relative)
{
  if (relative) {
    // Preserve path prefix (Python -R / --relative).
    std::filesystem::path rel = source;
    if (rel.is_absolute()) {
      rel = rel.relative_path(); // drop root ("/" or "C:/")
    }
    return target_dir / rel;
  }
  return target_dir / source.filename();
}

inline void print_result_items(const dirops::Result& result, std::string_view verb, bool verbose,
                               bool dry_run)
{
  if (!verbose && !dry_run) {
    return;
  }
  for (const auto& item : result.items) {
    if (item.skipped) {
      std::cout << "skip " << item.source.string() << '\n';
    } else if (item.destination.empty()) {
      std::cout << verb << ' ' << item.source.string() << '\n';
    } else {
      std::cout << verb << ' ' << item.source.string() << " -> " << item.destination.string()
                << '\n';
    }
  }
}

} // namespace dtcli

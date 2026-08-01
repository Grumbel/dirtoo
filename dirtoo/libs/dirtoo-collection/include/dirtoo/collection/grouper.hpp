// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"

#include <chrono>
#include <ctime>
#include <cstdio>
#include <string>

namespace dirtoo::collection {

/// How visible items are grouped (section headers in the UI).
enum class GroupMode {
  None,
  Day,        // local mtime YYYY-MM-DD (directories ungrouped)
  Directory,  // parent path of the entry
};

/// Sortable key for stable grouping (empty = ungrouped / directories under Day).
[[nodiscard]] inline std::string group_key(const fs::FileInfo& fi, GroupMode mode)
{
  switch (mode) {
  case GroupMode::None:
    return {};
  case GroupMode::Day: {
    if (fi.is_directory()) {
      return {}; // ungrouped, like Python DayGrouper
    }
    try {
      const auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(fi.mtime());
      const auto secs =
          std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
      if (secs == 0 && fi.is_synthetic()) {
        return "\x7f"; // sort last as unknown
      }
      std::time_t t = static_cast<std::time_t>(secs);
      std::tm tm{};
      localtime_r(&t, &tm);
      char buf[16];
      std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1,
                    tm.tm_mday);
      return buf;
    } catch (...) {
      return "\x7f";
    }
  }
  case GroupMode::Directory: {
    const auto parent = fi.path().parent_path();
    if (parent.empty()) {
      return "/";
    }
    return parent.generic_string();
  }
  }
  return {};
}

/// Human-readable section label (may differ slightly from key).
[[nodiscard]] inline std::string group_label(const fs::FileInfo& fi, GroupMode mode)
{
  switch (mode) {
  case GroupMode::None:
    return {};
  case GroupMode::Day: {
    const auto key = group_key(fi, mode);
    if (key.empty()) {
      return {}; // directories: no header of their own
    }
    if (key == "\x7f") {
      return "Unknown date";
    }
    return key;
  }
  case GroupMode::Directory: {
    const auto key = group_key(fi, mode);
    if (key.empty()) {
      return {};
    }
    return key;
  }
  }
  return {};
}

} // namespace dirtoo::collection

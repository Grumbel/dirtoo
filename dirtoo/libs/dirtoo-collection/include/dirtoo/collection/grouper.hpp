// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/filter/media_meta_cache.hpp"
#include "dirtoo/fs/file_info.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

namespace dirtoo::collection {

/// How visible items are grouped (section headers in the UI).
enum class GroupMode {
  None,
  Day,        // local mtime YYYY-MM-DD (directories ungrouped)
  Directory,  // parent path of the entry
  Duration,   // media duration buckets (Python DurationGrouper)
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
  case GroupMode::Duration: {
    if (fi.is_directory()) {
      return "9"; // after media buckets
    }
    // Memory cache only (no GUI-thread probe). Missing meta → unknown bucket.
    const auto meta = filter::MediaMetaCache::instance().try_get(fi.path());
    if (!meta || !meta->duration_ms || *meta->duration_ms == 0) {
      return "8"; // Unknown duration
    }
    const double minutes = static_cast<double>(*meta->duration_ms) / 60000.0;
    // Python buckets: >60, >30, >10, >5, else — keys sort long→short then unknown
    if (minutes > 60.0) {
      return "0";
    }
    if (minutes > 30.0) {
      return "1";
    }
    if (minutes > 10.0) {
      return "2";
    }
    if (minutes > 5.0) {
      return "3";
    }
    return "4";
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
      return {};
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
  case GroupMode::Duration: {
    const auto key = group_key(fi, mode);
    if (key == "0") {
      return "Very Long (>60 minutes)";
    }
    if (key == "1") {
      return "Long (< 60 minutes)";
    }
    if (key == "2") {
      return "Medium (< 30 minutes)";
    }
    if (key == "3") {
      return "Short (< 10 minutes)";
    }
    if (key == "4") {
      return "Very Short (< 5 minutes)";
    }
    if (key == "9") {
      return "Directories";
    }
    return "Unknown duration";
  }
  }
  return {};
}

} // namespace dirtoo::collection

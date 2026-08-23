// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/filter/media_meta_cache.hpp"
#include "dirtoo/fs/file_info.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

namespace dirtoo::collection {

/// How visible items are grouped (section headers in the UI).
enum class GroupMode {
  None,
  Day,        // local mtime YYYY-MM-DD (directories ungrouped)
  Directory,  // parent path of the entry
  Duration,   // media duration buckets (Python DurationGrouper)
  Session,    // clusters by mtime gaps (≥10h starts a new session)
};

/// Default gap that starts a new session group (matches UI “time gaps” scale).
inline constexpr std::chrono::seconds kSessionGapThreshold{10 * 60 * 60};

namespace detail {

[[nodiscard]] inline std::optional<std::int64_t> mtime_epoch_sec(const fs::FileInfo& fi)
{
  try {
    const auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(fi.mtime());
    return std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
  } catch (...) {
    return std::nullopt;
  }
}

[[nodiscard]] inline std::string format_local_ymd(std::int64_t secs)
{
  std::time_t t = static_cast<std::time_t>(secs);
  std::tm tm{};
  localtime_r(&t, &tm);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
  return buf;
}

[[nodiscard]] inline std::string format_local_ymd_hm(std::int64_t secs)
{
  std::time_t t = static_cast<std::time_t>(secs);
  std::tm tm{};
  localtime_r(&t, &tm);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1,
                tm.tm_mday, tm.tm_hour, tm.tm_min);
  return buf;
}

} // namespace detail

/// Sortable key for stable grouping (empty = ungrouped / directories under Day).
/// Not used for Session (session ids need a global pass — see apply_grouping).
[[nodiscard]] inline std::string group_key(const fs::FileInfo& fi, GroupMode mode)
{
  switch (mode) {
  case GroupMode::None:
  case GroupMode::Session:
    return {};
  case GroupMode::Day: {
    if (fi.is_directory()) {
      return {}; // ungrouped, like Python DayGrouper
    }
    const auto secs = detail::mtime_epoch_sec(fi);
    if (!secs || (*secs == 0 && fi.is_synthetic())) {
      return "\x7f"; // sort last as unknown
    }
    return detail::format_local_ymd(*secs);
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
  case GroupMode::Session:
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

/// Reorder @p items for the group mode and return per-item section labels
/// (empty string = no header for that row). Labels are computed once so the
/// GUI does not re-run localtime/path work on every paint/sizeHint.
[[nodiscard]] inline std::vector<std::string> apply_grouping(std::vector<fs::FileInfo>& items,
                                                             GroupMode mode)
{
  std::vector<std::string> labels(items.size());
  if (mode == GroupMode::None || items.empty()) {
    return labels;
  }

  if (mode == GroupMode::Session) {
    // Newest first so “sessions” read chronologically downward in the UI.
    std::stable_sort(items.begin(), items.end(), [](const fs::FileInfo& a, const fs::FileInfo& b) {
      const auto sa = detail::mtime_epoch_sec(a).value_or(0);
      const auto sb = detail::mtime_epoch_sec(b).value_or(0);
      if (sa != sb) {
        return sa > sb;
      }
      return a.path().generic_string() < b.path().generic_string();
    });

    const auto gap_secs = kSessionGapThreshold.count();
    std::int64_t session_start = detail::mtime_epoch_sec(items.front()).value_or(0);
    std::int64_t prev = session_start;
    std::string current_label =
        session_start != 0 ? ("Session " + detail::format_local_ymd_hm(session_start))
                           : std::string{"Session (unknown time)"};
    labels[0] = current_label;

    for (std::size_t i = 1; i < items.size(); ++i) {
      const auto cur = detail::mtime_epoch_sec(items[i]).value_or(0);
      // Sorted newest→oldest: gap is prev - cur.
      if (prev != 0 && cur != 0 && (prev - cur) >= gap_secs) {
        session_start = cur;
        current_label = "Session " + detail::format_local_ymd_hm(session_start);
      } else if (cur == 0 && prev != 0) {
        current_label = "Session (unknown time)";
        session_start = 0;
      }
      labels[i] = current_label;
      prev = cur != 0 ? cur : prev;
    }
    return labels;
  }

  if (items.size() > 1) {
    std::stable_sort(items.begin(), items.end(), [mode](const fs::FileInfo& a, const fs::FileInfo& b) {
      return group_key(a, mode) < group_key(b, mode);
    });
  }
  for (std::size_t i = 0; i < items.size(); ++i) {
    labels[i] = group_label(items[i], mode);
  }
  return labels;
}

} // namespace dirtoo::collection

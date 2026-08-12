// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Internal helpers shared by predicates_*.cpp translation units.

#include "dirtoo/filter/filter_item.hpp"
#include "dirtoo/filter/media_meta_cache.hpp"
#include "dirtoo/filter/media_probe.hpp"

#include <cctype>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace dirtoo::filter::detail {

[[nodiscard]] inline std::optional<MediaInfo> lookup_media(const std::filesystem::path& path)
{
  auto& cache = MediaMetaCache::instance();
  if (auto hit = cache.try_get(path)) {
    return hit;
  }
  if (cache.is_negative(path)) {
    return std::nullopt;
  }
  // CLI / non-GUI: resolve synchronously via workers+SQLite (not for paint).
  return resolve_media_cached(path);
}

[[nodiscard]] inline std::string lower_copy(std::string s)
{
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

[[nodiscard]] inline std::optional<std::int64_t> resolve_mtime_sec(const FilterItem& item)
{
  if (item.mtime_sec) {
    return item.mtime_sec;
  }
  if (item.path.empty()) {
    return std::nullopt;
  }
  std::error_code ec;
  const auto ft = std::filesystem::last_write_time(item.path, ec);
  if (ec) {
    return std::nullopt;
  }
  const auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ft);
  return std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
}

enum class LenCmp { Eq, Ne, Lt, Le, Gt, Ge };

[[nodiscard]] inline std::pair<LenCmp, std::string_view> split_len_cmp(std::string_view arg)
{
  while (!arg.empty() && std::isspace(static_cast<unsigned char>(arg.front()))) {
    arg.remove_prefix(1);
  }
  if (arg.starts_with(">=")) {
    return {LenCmp::Ge, arg.substr(2)};
  }
  if (arg.starts_with("<=")) {
    return {LenCmp::Le, arg.substr(2)};
  }
  if (arg.starts_with("!=") || arg.starts_with("<>")) {
    return {LenCmp::Ne, arg.substr(2)};
  }
  if (arg.starts_with(">")) {
    return {LenCmp::Gt, arg.substr(1)};
  }
  if (arg.starts_with("<")) {
    return {LenCmp::Lt, arg.substr(1)};
  }
  if (arg.starts_with("=")) {
    return {LenCmp::Eq, arg.substr(1)};
  }
  return {LenCmp::Eq, arg};
}

[[nodiscard]] inline bool apply_len_cmp(LenCmp op, double a, double b)
{
  switch (op) {
  case LenCmp::Eq:
    return a == b;
  case LenCmp::Ne:
    return a != b;
  case LenCmp::Lt:
    return a < b;
  case LenCmp::Le:
    return a <= b;
  case LenCmp::Gt:
    return a > b;
  case LenCmp::Ge:
    return a >= b;
  }
  return false;
}


/// Split inclusive range syntax: "lo-hi" or "lo..hi".
/// Returns nullopt if the argument uses a comparison operator or is not a range.
[[nodiscard]] inline std::optional<std::pair<std::string_view, std::string_view>>
split_range_arg(std::string_view arg)
{
  while (!arg.empty() && std::isspace(static_cast<unsigned char>(arg.front()))) {
    arg.remove_prefix(1);
  }
  while (!arg.empty() && std::isspace(static_cast<unsigned char>(arg.back()))) {
    arg.remove_suffix(1);
  }
  if (arg.empty()) {
    return std::nullopt;
  }
  // Comparison forms are not ranges.
  if (arg.starts_with(">=") || arg.starts_with("<=") || arg.starts_with("!=")
      || arg.starts_with("<>") || arg.starts_with(">") || arg.starts_with("<")
      || arg.starts_with("=")) {
    return std::nullopt;
  }
  // Prefer ".." (unambiguous) over "-".
  if (const auto dots = arg.find(".."); dots != std::string_view::npos && dots > 0) {
    auto lo = arg.substr(0, dots);
    auto hi = arg.substr(dots + 2);
    while (!lo.empty() && std::isspace(static_cast<unsigned char>(lo.back()))) {
      lo.remove_suffix(1);
    }
    while (!hi.empty() && std::isspace(static_cast<unsigned char>(hi.front()))) {
      hi.remove_prefix(1);
    }
    if (!lo.empty() && !hi.empty()) {
      return std::pair{lo, hi};
    }
    return std::nullopt;
  }
  // Single '-': left side must start with a digit (or '.') so we do not treat
  // leading-minus forms as ranges.
  if (const auto dash = arg.find('-'); dash != std::string_view::npos && dash > 0) {
    const char first = arg.front();
    if (!(std::isdigit(static_cast<unsigned char>(first)) || first == '.')) {
      return std::nullopt;
    }
    auto lo = arg.substr(0, dash);
    auto hi = arg.substr(dash + 1);
    while (!lo.empty() && std::isspace(static_cast<unsigned char>(lo.back()))) {
      lo.remove_suffix(1);
    }
    while (!hi.empty() && std::isspace(static_cast<unsigned char>(hi.front()))) {
      hi.remove_prefix(1);
    }
    if (!lo.empty() && !hi.empty()) {
      return std::pair{lo, hi};
    }
  }
  return std::nullopt;
}

} // namespace dirtoo::filter::detail

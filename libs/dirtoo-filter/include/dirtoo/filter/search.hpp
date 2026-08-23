// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/filter/filter_item.hpp"
#include "dirtoo/filter/match_func.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace dirtoo::filter {

struct SearchOptions {
  /// Maximum directory depth below root (0 = only root itself, -1 = unlimited).
  int max_depth = -1;
  bool follow_directory_symlinks = false;
  /// When false, skip names starting with '.'.
  bool show_hidden = false;
  /// Optional cooperative cancel; polled between entries.
  std::function<bool()> should_cancel;
};

struct SearchStats {
  std::uint64_t visited = 0;
  std::uint64_t matched = 0;
  std::uint64_t errors = 0;
};

/// Walk `root` (non-recursive if max_depth == 0) and invoke `on_match` for each
/// entry that satisfies `match`. Directories and files are both considered.
/// Returns stats; never throws (errors increment stats.errors).
[[nodiscard]] SearchStats
search_directory(const std::filesystem::path& root, const MatchFunc& match,
                 const SearchOptions& options,
                 const std::function<void(const FilterItem&)>& on_match);

/// Convenience: collect matching items into a vector (capped by `limit` if set).
[[nodiscard]] std::vector<FilterItem>
search_directory_collect(const std::filesystem::path& root, const MatchFunc& match,
                         const SearchOptions& options,
                         std::optional<std::size_t> limit = std::nullopt);

} // namespace dirtoo::filter

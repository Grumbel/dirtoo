// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/filter/match_func.hpp"

#include <cstddef>
#include <cstdint>
#include <regex>
#include <string>

namespace dirtoo::filter {

[[nodiscard]] MatchFuncPtr make_name_substring(std::string needle, bool case_sensitive = false);
[[nodiscard]] MatchFuncPtr make_glob(std::string pattern, bool case_sensitive = false);
[[nodiscard]] MatchFuncPtr make_regex(std::string pattern, bool case_sensitive = false);
[[nodiscard]] MatchFuncPtr make_type(std::string argument); // file|dir|folder|link|...
[[nodiscard]] MatchFuncPtr make_size(std::string argument); // e.g. >1M, <=100k, 10K-2M
[[nodiscard]] MatchFuncPtr make_width(std::string argument);  // e.g. >=1920, =640
[[nodiscard]] MatchFuncPtr make_height(std::string argument);
[[nodiscard]] MatchFuncPtr make_duration(std::string argument); // seconds; >1m, 1:30
[[nodiscard]] MatchFuncPtr make_framerate(std::string argument);
/// N-gram fuzzy basename match (threshold default 0.5, n=3). Arg: needle or needle@0.6
[[nodiscard]] MatchFuncPtr make_fuzzy(std::string argument, bool case_sensitive = false);
[[nodiscard]] double fuzzy_score(std::string_view needle, std::string_view haystack,
                                int n = 3, bool case_sensitive = false);

/// Basename character length compare: length:>10, len:=3
[[nodiscard]] MatchFuncPtr make_length(std::string argument);

/// mtime date: today | >=2024-01-01 | 2024-*-01 | 2024-06 | 2024
[[nodiscard]] MatchFuncPtr make_date(std::string argument);

/// File content substring (UTF-8, errors replaced). Default max 1 MiB read.
/// contains: needle (case-insensitive), Contains: needle (case-sensitive).
[[nodiscard]] MatchFuncPtr make_contains(std::string argument, bool case_sensitive = false,
                                         std::size_t max_bytes = 1u << 20);
/// File content regex (ECMAScript). containsre: / Containsre: (case-sensitive).
[[nodiscard]] MatchFuncPtr make_contains_regex(std::string argument, bool case_sensitive = false,
                                               std::size_t max_bytes = 1u << 20);

/// mtime time-of-day: time:>=15:00, time:9:30 (HH:MM local)
[[nodiscard]] MatchFuncPtr make_time(std::string argument);
/// weekday: monday | mon | 0–6 with optional compare (weekday:>=fri)
[[nodiscard]] MatchFuncPtr make_weekday(std::string argument);

} // namespace dirtoo::filter

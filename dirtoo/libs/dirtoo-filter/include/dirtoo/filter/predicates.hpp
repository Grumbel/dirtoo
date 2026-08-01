// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/filter/match_func.hpp"

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

} // namespace dirtoo::filter

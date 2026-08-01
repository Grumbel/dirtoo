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

} // namespace dirtoo::filter

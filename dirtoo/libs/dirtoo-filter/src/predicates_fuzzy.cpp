// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/predicates.hpp"

#include "dirtoo/filter/media_meta_cache.hpp"

#include <algorithm>
#include <fstream>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <tuple>
#include <set>
#include <cctype>
#include <charconv>
#include <optional>
#include <vector>
#include <map>
#include <mutex>
#include <random>
#include <filesystem>
#include <string>
#include <string_view>
#include <regex>
#include <system_error>

namespace dirtoo::filter {
double fuzzy_score(std::string_view needle, std::string_view haystack, int n, bool case_sensitive)
{
  if (needle.empty()) {
    return 1.0;
  }
  if (haystack.empty()) {
    return 0.0;
  }

  auto norm = [case_sensitive](std::string_view s) {
    if (case_sensitive) {
      return std::string{s};
    }
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
      out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
  };
  const std::string a = norm(needle);
  const std::string b = norm(haystack);

  const int nn = std::max(1, std::min(n, static_cast<int>(a.size())));
  if (static_cast<int>(a.size()) < nn) {
    return b.find(a) != std::string::npos ? 1.0 : 0.0;
  }

  std::set<std::string> needle_grams;
  for (std::size_t i = 0; i + static_cast<std::size_t>(nn) <= a.size(); ++i) {
    needle_grams.insert(a.substr(i, static_cast<std::size_t>(nn)));
  }
  if (needle_grams.empty()) {
    return 0.0;
  }

  std::set<std::string> hay_grams;
  if (static_cast<int>(b.size()) >= nn) {
    for (std::size_t i = 0; i + static_cast<std::size_t>(nn) <= b.size(); ++i) {
      hay_grams.insert(b.substr(i, static_cast<std::size_t>(nn)));
    }
  }

  std::size_t matches = 0;
  for (const auto& g : needle_grams) {
    if (hay_grams.contains(g)) {
      ++matches;
    }
  }
  return static_cast<double>(matches) / static_cast<double>(needle_grams.size());
}

namespace {

class FuzzyMatch : public MatchFunc {
public:
  FuzzyMatch(std::string needle, double threshold, int n, bool case_sensitive)
      : needle_(std::move(needle))
      , threshold_(threshold)
      , n_(n)
      , case_sensitive_(case_sensitive)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    return fuzzy_score(needle_, item.name, n_, case_sensitive_) > threshold_;
  }

private:
  std::string needle_;
  double threshold_;
  int n_;
  bool case_sensitive_;
};

} // namespace

MatchFuncPtr make_fuzzy(std::string argument, bool case_sensitive)
{
  // Forms: "needle" | "needle@0.6" | "needle@0.6@2" (threshold, optional n)
  double threshold = 0.5;
  int n = 3;
  std::string needle = std::move(argument);

  // Optional @n then @threshold — parse trailing @n if integer-looking after threshold attempt
  {
    const auto at = needle.rfind('@');
    if (at != std::string::npos && at + 1 < needle.size()) {
      const std::string tail = needle.substr(at + 1);
      bool is_int = !tail.empty();
      for (char c : tail) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
          is_int = false;
          break;
        }
      }
      if (is_int && tail.size() <= 2) {
        try {
          n = std::stoi(tail);
          needle.resize(at);
        } catch (...) {
        }
      }
    }
  }
  {
    const auto at = needle.rfind('@');
    if (at != std::string::npos && at + 1 < needle.size()) {
      try {
        threshold = std::stod(needle.substr(at + 1));
        needle.resize(at);
      } catch (...) {
      }
    }
  }

  if (needle.empty()) {
    return std::make_shared<AlwaysFalse>();
  }
  if (n < 1) {
    n = 1;
  }
  if (threshold < 0.0) {
    threshold = 0.0;
  }
  if (threshold > 1.0) {
    threshold = 1.0;
  }
  return std::make_shared<FuzzyMatch>(std::move(needle), threshold, n, case_sensitive);
}

} // namespace dirtoo::filter

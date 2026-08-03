// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/predicates.hpp"
#include "predicates_detail.hpp"

#include <algorithm>
#include <fstream>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cctype>
#include <optional>
#include <vector>
#include <string>
#include <string_view>
#include <regex>
#include <filesystem>
#include <system_error>
#include <random>
#include <cstdlib>

namespace dirtoo::filter {
using detail::LenCmp;
using detail::split_len_cmp;
using detail::apply_len_cmp;
using detail::lower_copy;
using detail::resolve_mtime_sec;
using detail::lookup_media;

namespace {

class ContainsMatch : public MatchFunc {
public:
  ContainsMatch(std::string needle, bool case_sensitive, std::size_t max_bytes)
      : needle_(std::move(needle))
      , case_sensitive_(case_sensitive)
      , max_bytes_(max_bytes)
  {
    if (!case_sensitive_) {
      needle_ = lower_copy(std::move(needle_));
    }
  }
  bool matches(const FilterItem& item) const override
  {
    if (item.is_directory || item.path.empty() || needle_.empty()) {
      return false;
    }
    std::ifstream in(item.path, std::ios::binary);
    if (!in) {
      return false;
    }
    constexpr std::size_t kChunk = 64 * 1024;
    std::string chunk(kChunk, '\0');
    std::size_t total = 0;
    std::string carry;
    while (in && total < max_bytes_) {
      const auto to_read = std::min(kChunk, max_bytes_ - total);
      in.read(chunk.data(), static_cast<std::streamsize>(to_read));
      const auto n = static_cast<std::size_t>(in.gcount());
      if (n == 0) {
        break;
      }
      total += n;
      std::string view = carry;
      view.append(chunk.data(), n);
      if (case_sensitive_) {
        if (view.find(needle_) != std::string::npos) {
          return true;
        }
      } else if (lower_copy(view).find(needle_) != std::string::npos) {
        return true;
      }
      const std::size_t keep =
          needle_.empty() ? 0 : std::min(view.size(), needle_.size() - 1);
      carry = view.substr(view.size() - keep);
    }
    return false;
  }

private:
  std::string needle_;
  bool case_sensitive_;
  std::size_t max_bytes_;
};


class ContainsRegexMatch : public MatchFunc {
public:
  ContainsRegexMatch(std::regex re, std::size_t max_bytes)
      : re_(std::move(re))
      , max_bytes_(max_bytes)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    if (item.is_directory || item.path.empty()) {
      return false;
    }
    std::ifstream in(item.path, std::ios::binary);
    if (!in) {
      return false;
    }
    constexpr std::size_t kChunk = 64 * 1024;
    std::string chunk(kChunk, '\0');
    std::size_t total = 0;
    std::string carry;
    // Overlap keep: up to 4KiB so multi-line patterns spanning chunks can still match.
    constexpr std::size_t kOverlap = 4096;
    while (in && total < max_bytes_) {
      const auto to_read = std::min(kChunk, max_bytes_ - total);
      in.read(chunk.data(), static_cast<std::streamsize>(to_read));
      const auto n = static_cast<std::size_t>(in.gcount());
      if (n == 0) {
        break;
      }
      total += n;
      std::string view = carry;
      view.append(chunk.data(), n);
      try {
        if (std::regex_search(view, re_)) {
          return true;
        }
      } catch (...) {
        return false;
      }
      if (view.size() > kOverlap) {
        carry = view.substr(view.size() - kOverlap);
      } else {
        carry = std::move(view);
      }
    }
    return false;
  }

private:
  std::regex re_;
  std::size_t max_bytes_;
};


class ContainsFuzzyMatch : public MatchFunc {
public:
  ContainsFuzzyMatch(std::string needle, double threshold, int n, std::size_t max_bytes)
      : needle_(std::move(needle))
      , threshold_(threshold)
      , n_(n)
      , max_bytes_(max_bytes)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    if (item.is_directory || item.path.empty() || needle_.empty()) {
      return false;
    }
    std::ifstream in(item.path, std::ios::binary);
    if (!in) {
      return false;
    }
    constexpr std::size_t kChunk = 64 * 1024;
    std::string chunk(kChunk, '\0');
    std::size_t total = 0;
    std::string line_carry;
    while (in && total < max_bytes_) {
      const auto to_read = std::min(kChunk, max_bytes_ - total);
      in.read(chunk.data(), static_cast<std::streamsize>(to_read));
      const auto got = static_cast<std::size_t>(in.gcount());
      if (got == 0) {
        break;
      }
      total += got;
      std::string buf = line_carry;
      buf.append(chunk.data(), got);
      std::size_t start = 0;
      while (start < buf.size()) {
        const auto nl = buf.find('\n', start);
        if (nl == std::string::npos) {
          line_carry = buf.substr(start);
          break;
        }
        std::string_view line{buf.data() + start, nl - start};
        if (!line.empty() && line.back() == '\r') {
          line.remove_suffix(1);
        }
        if (fuzzy_score(needle_, line, n_, false) > threshold_) {
          return true;
        }
        start = nl + 1;
        line_carry.clear();
      }
      if (start >= buf.size()) {
        line_carry.clear();
      }
    }
    if (!line_carry.empty() && fuzzy_score(needle_, line_carry, n_, false) > threshold_) {
      return true;
    }
    return false;
  }

private:
  std::string needle_;
  double threshold_;
  int n_;
  std::size_t max_bytes_;
};


} // namespace

MatchFuncPtr make_contains(std::string argument, bool case_sensitive, std::size_t max_bytes)
{
  if (argument.empty()) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<ContainsMatch>(std::move(argument), case_sensitive, max_bytes);
}

MatchFuncPtr make_contains_regex(std::string argument, bool case_sensitive, std::size_t max_bytes)
{
  if (argument.empty()) {
    return std::make_shared<AlwaysFalse>();
  }
  try {
    const auto flags =
        case_sensitive ? std::regex::ECMAScript
                       : (std::regex::ECMAScript | std::regex::icase);
    return std::make_shared<ContainsRegexMatch>(std::regex(argument, flags), max_bytes);
  } catch (const std::regex_error&) {
    return std::make_shared<AlwaysFalse>();
  }
}




MatchFuncPtr make_contains_fuzzy(std::string argument, std::size_t max_bytes)
{
  // Same optional @threshold / @n suffixes as make_fuzzy
  double threshold = 0.5;
  int n = 3;
  std::string needle = std::move(argument);
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
  threshold = std::clamp(threshold, 0.0, 1.0);
  return std::make_shared<ContainsFuzzyMatch>(std::move(needle), threshold, n, max_bytes);
}


} // namespace dirtoo::filter

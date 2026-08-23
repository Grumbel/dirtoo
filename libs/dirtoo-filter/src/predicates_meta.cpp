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
#include <tuple>

namespace dirtoo::filter {
using detail::LenCmp;
using detail::split_len_cmp;
using detail::apply_len_cmp;
using detail::lower_copy;
using detail::resolve_mtime_sec;
using detail::lookup_media;

namespace {

class LengthMatch : public MatchFunc {
public:
  LengthMatch(LenCmp op, std::size_t value)
      : range_(false)
      , op_(op)
      , lo_(value)
      , hi_(value)
  {
  }
  LengthMatch(std::size_t lo, std::size_t hi)
      : range_(true)
      , op_(LenCmp::Eq)
      , lo_(std::min(lo, hi))
      , hi_(std::max(lo, hi))
  {
  }
  bool matches(const FilterItem& item) const override
  {
    const auto n = item.name.size();
    if (range_) {
      return n >= lo_ && n <= hi_;
    }
    return apply_len_cmp(op_, static_cast<double>(n), static_cast<double>(lo_));
  }

private:
  bool range_;
  LenCmp op_;
  std::size_t lo_;
  std::size_t hi_;
};

struct DateKey {
  int year = 0;
  int month = 0;
  int day = 0;
  [[nodiscard]] auto tie() const { return std::tuple{year, month, day}; }
  bool operator==(const DateKey& o) const { return tie() == o.tie(); }
  bool operator!=(const DateKey& o) const { return tie() != o.tie(); }
  bool operator<(const DateKey& o) const { return tie() < o.tie(); }
  bool operator<=(const DateKey& o) const { return tie() <= o.tie(); }
  bool operator>(const DateKey& o) const { return tie() > o.tie(); }
  bool operator>=(const DateKey& o) const { return tie() >= o.tie(); }
};

std::optional<DateKey> parse_date_key(std::string_view text)
{
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
    text.remove_prefix(1);
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
    text.remove_suffix(1);
  }
  if (text.empty()) {
    return std::nullopt;
  }
  try {
    if (text.size() >= 10 && text[4] == '-' && text[7] == '-') {
      DateKey k;
      k.year = std::stoi(std::string{text.substr(0, 4)});
      k.month = std::stoi(std::string{text.substr(5, 2)});
      k.day = std::stoi(std::string{text.substr(8, 2)});
      return k;
    }
    if (text.size() >= 7 && text[4] == '-') {
      DateKey k;
      k.year = std::stoi(std::string{text.substr(0, 4)});
      k.month = std::stoi(std::string{text.substr(5, 2)});
      return k;
    }
    if (text.size() == 4) {
      DateKey k;
      k.year = std::stoi(std::string{text});
      return k;
    }
  } catch (...) {
  }
  return std::nullopt;
}

DateKey date_key_from_mtime(std::int64_t mtime_sec, int precision)
{
  std::time_t t = static_cast<std::time_t>(mtime_sec);
  std::tm tm{};
  localtime_r(&t, &tm);
  DateKey k;
  k.year = tm.tm_year + 1900;
  if (precision >= 1) {
    k.month = tm.tm_mon + 1;
  }
  if (precision >= 2) {
    k.day = tm.tm_mday;
  }
  return k;
}

int date_precision(const DateKey& k)
{
  if (k.day > 0) {
    return 2;
  }
  if (k.month > 0) {
    return 1;
  }
  return 0;
}

bool apply_cmp_date(LenCmp op, const DateKey& a, const DateKey& b)
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

class DateOpMatch : public MatchFunc {
public:
  DateOpMatch(LenCmp op, DateKey key)
      : op_(op)
      , key_(key)
      , precision_(date_precision(key))
  {
  }
  bool matches(const FilterItem& item) const override
  {
    const auto mt = resolve_mtime_sec(item);
    if (!mt) {
      return false;
    }
    const DateKey actual = date_key_from_mtime(*mt, precision_);
    return apply_cmp_date(op_, actual, key_);
  }

private:
  LenCmp op_;
  DateKey key_;
  int precision_;
};

bool date_glob_match(std::string_view text, std::string_view pattern)
{
  std::size_t ti = 0;
  std::size_t pi = 0;
  std::size_t star = std::string_view::npos;
  std::size_t match = 0;
  while (ti < text.size()) {
    if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == text[ti])) {
      ++ti;
      ++pi;
    } else if (pi < pattern.size() && pattern[pi] == '*') {
      star = pi++;
      match = ti;
    } else if (star != std::string_view::npos) {
      pi = star + 1;
      ti = ++match;
    } else {
      return false;
    }
  }
  while (pi < pattern.size() && pattern[pi] == '*') {
    ++pi;
  }
  return pi == pattern.size();
}

class DateGlobMatch : public MatchFunc {
public:
  explicit DateGlobMatch(std::string pattern)
      : pattern_(std::move(pattern))
  {
  }
  bool matches(const FilterItem& item) const override
  {
    const auto mt = resolve_mtime_sec(item);
    if (!mt) {
      return false;
    }
    const DateKey k = date_key_from_mtime(*mt, 2);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", k.year, k.month, k.day);
    return date_glob_match(buf, pattern_);
  }

private:
  std::string pattern_;
};


} // namespace

MatchFuncPtr make_length(std::string argument)
{
  if (const auto rng = detail::split_range_arg(argument)) {
    try {
      const auto lo = static_cast<std::size_t>(std::stoull(std::string{rng->first}));
      const auto hi = static_cast<std::size_t>(std::stoull(std::string{rng->second}));
      return std::make_shared<LengthMatch>(lo, hi);
    } catch (...) {
      return std::make_shared<AlwaysFalse>();
    }
  }
  const auto [op, rest] = split_len_cmp(argument);
  auto trimmed = rest;
  while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
    trimmed.remove_prefix(1);
  }
  if (trimmed.empty()) {
    return std::make_shared<AlwaysFalse>();
  }
  try {
    const auto value = static_cast<std::size_t>(std::stoull(std::string{trimmed}));
    return std::make_shared<LengthMatch>(op, value);
  } catch (...) {
    return std::make_shared<AlwaysFalse>();
  }
}

MatchFuncPtr make_date(std::string argument)
{
  while (!argument.empty() && std::isspace(static_cast<unsigned char>(argument.front()))) {
    argument.erase(argument.begin());
  }
  while (!argument.empty() && std::isspace(static_cast<unsigned char>(argument.back()))) {
    argument.pop_back();
  }
  if (argument.empty()) {
    return std::make_shared<AlwaysFalse>();
  }
  if (lower_copy(argument) == "today") {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);
    DateKey k;
    k.year = tm.tm_year + 1900;
    k.month = tm.tm_mon + 1;
    k.day = tm.tm_mday;
    return std::make_shared<DateOpMatch>(LenCmp::Ge, k);
  }
  if (argument.find('*') != std::string::npos || argument.find('?') != std::string::npos) {
    return std::make_shared<DateGlobMatch>(std::move(argument));
  }
  const auto [op, rest] = split_len_cmp(argument);
  const auto key = parse_date_key(rest);
  if (!key) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<DateOpMatch>(op, *key);
}


namespace {

struct TimeOfDay {
  int minutes = 0; // minutes since midnight
};

std::optional<TimeOfDay> parse_time_of_day(std::string_view text)
{
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
    text.remove_prefix(1);
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
    text.remove_suffix(1);
  }
  if (text.empty()) {
    return std::nullopt;
  }
  const auto colon = text.find(':');
  try {
    if (colon == std::string_view::npos) {
      const int h = std::stoi(std::string{text});
      if (h < 0 || h > 23) {
        return std::nullopt;
      }
      return TimeOfDay{h * 60};
    }
    const int h = std::stoi(std::string{text.substr(0, colon)});
    const int m = std::stoi(std::string{text.substr(colon + 1)});
    if (h < 0 || h > 23 || m < 0 || m > 59) {
      return std::nullopt;
    }
    return TimeOfDay{h * 60 + m};
  } catch (...) {
    return std::nullopt;
  }
}

TimeOfDay time_from_mtime(std::int64_t mtime_sec)
{
  std::time_t tt = static_cast<std::time_t>(mtime_sec);
  std::tm tm{};
  localtime_r(&tt, &tm);
  return TimeOfDay{tm.tm_hour * 60 + tm.tm_min};
}

class TimeOpMatch : public MatchFunc {
public:
  TimeOpMatch(LenCmp op, TimeOfDay tod)
      : op_(op)
      , tod_(tod)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    const auto mt = resolve_mtime_sec(item);
    if (!mt) {
      return false;
    }
    const auto actual = time_from_mtime(*mt);
    return apply_len_cmp(op_, static_cast<double>(actual.minutes), static_cast<double>(tod_.minutes));
  }

private:
  LenCmp op_;
  TimeOfDay tod_;
};

std::optional<int> parse_weekday(std::string_view text)
{
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
    text.remove_prefix(1);
  }
  if (text.empty()) {
    return std::nullopt;
  }
  // numeric 0=Mon … 6=Sun (matches Python datetime.weekday)
  bool all_digit = true;
  for (char c : text) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      all_digit = false;
      break;
    }
  }
  if (all_digit) {
    try {
      const int v = std::stoi(std::string{text});
      if (v < 0 || v > 6) {
        return std::nullopt;
      }
      return v;
    } catch (...) {
      return std::nullopt;
    }
  }
  std::string lower = lower_copy(std::string{text});
  static constexpr const char* names[] = {
      "monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday",
  };
  std::vector<int> hits;
  for (int i = 0; i < 7; ++i) {
    std::string n = names[i];
    if (n.starts_with(lower) || lower.starts_with(n.substr(0, std::min(lower.size(), n.size())))) {
      // prefix match either way for short forms mon/tue/...
      if (n.starts_with(lower)) {
        hits.push_back(i);
      }
    }
  }
  // also explicit short forms
  if (hits.empty()) {
    static constexpr const char* shorts[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
    for (int i = 0; i < 7; ++i) {
      if (lower == shorts[i] || std::string{shorts[i]}.starts_with(lower)) {
        hits.push_back(i);
      }
    }
  }
  if (hits.size() == 1) {
    return hits.front();
  }
  return std::nullopt;
}

class WeekdayMatch : public MatchFunc {
public:
  WeekdayMatch(LenCmp op, int weekday)
      : op_(op)
      , weekday_(weekday)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    const auto mt = resolve_mtime_sec(item);
    if (!mt) {
      return false;
    }
    std::time_t tt = static_cast<std::time_t>(*mt);
    std::tm tm{};
    localtime_r(&tt, &tm);
    // tm_wday: 0=Sunday → convert to Python Monday=0
    const int py = (tm.tm_wday + 6) % 7;
    return apply_len_cmp(op_, static_cast<double>(py), static_cast<double>(weekday_));
  }

private:
  LenCmp op_;
  int weekday_;
};

} // namespace

MatchFuncPtr make_time(std::string argument)
{
  const auto [op, rest] = split_len_cmp(argument);
  const auto tod = parse_time_of_day(rest);
  if (!tod) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<TimeOpMatch>(op, *tod);
}

MatchFuncPtr make_weekday(std::string argument)
{
  const auto [op, rest] = split_len_cmp(argument);
  const auto wd = parse_weekday(rest);
  if (!wd) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<WeekdayMatch>(op, *wd);
}

} // namespace dirtoo::filter

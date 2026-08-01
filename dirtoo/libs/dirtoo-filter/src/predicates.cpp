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

namespace dirtoo::filter {
namespace {

std::string to_lower(std::string s)
{
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

bool match_glob_impl(std::string_view text, std::string_view pattern, bool case_sensitive)
{
  std::size_t ti = 0;
  std::size_t pi = 0;
  std::size_t star = std::string_view::npos;
  std::size_t match = 0;

  auto eq = [case_sensitive](char a, char b) {
    if (case_sensitive) {
      return a == b;
    }
    return std::tolower(static_cast<unsigned char>(a))
           == std::tolower(static_cast<unsigned char>(b));
  };

  while (ti < text.size()) {
    if (pi < pattern.size()
        && (pattern[pi] == '?' || eq(pattern[pi], text[ti]))) {
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

std::optional<std::uint64_t> parse_size_token(std::string_view s)
{
  if (s.empty()) {
    return std::nullopt;
  }
  std::size_t i = 0;
  while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.')) {
    ++i;
  }
  if (i == 0) {
    return std::nullopt;
  }
  double value = 0;
  {
    const std::string num{s.substr(0, i)};
    try {
      value = std::stod(num);
    } catch (...) {
      return std::nullopt;
    }
  }
  std::string unit = to_lower(std::string{s.substr(i)});
  double mult = 1.0;
  if (unit.empty() || unit == "b") {
    mult = 1.0;
  } else if (unit == "k" || unit == "kb") {
    mult = 1024.0;
  } else if (unit == "m" || unit == "mb") {
    mult = 1024.0 * 1024.0;
  } else if (unit == "g" || unit == "gb") {
    mult = 1024.0 * 1024.0 * 1024.0;
  } else if (unit == "t" || unit == "tb") {
    mult = 1024.0 * 1024.0 * 1024.0 * 1024.0;
  } else {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(value * mult);
}

class NameSubstringMatch : public MatchFunc {
public:
  NameSubstringMatch(std::string needle, bool case_sensitive)
      : needle_(case_sensitive ? std::move(needle) : to_lower(std::move(needle)))
      , case_sensitive_(case_sensitive)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    if (case_sensitive_) {
      return item.name.find(needle_) != std::string::npos;
    }
    return to_lower(item.name).find(needle_) != std::string::npos;
  }

private:
  std::string needle_;
  bool case_sensitive_;
};

class GlobMatch : public MatchFunc {
public:
  GlobMatch(std::string pattern, bool case_sensitive)
      : pattern_(std::move(pattern))
      , case_sensitive_(case_sensitive)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    return match_glob_impl(item.name, pattern_, case_sensitive_);
  }

private:
  std::string pattern_;
  bool case_sensitive_;
};

class RegexMatch : public MatchFunc {
public:
  RegexMatch(std::regex re)
      : re_(std::move(re))
  {
  }
  bool matches(const FilterItem& item) const override
  {
    return std::regex_search(item.name, re_);
  }

private:
  std::regex re_;
};

class TypeMatch : public MatchFunc {
public:
  enum class Kind { File, Dir, Any };
  explicit TypeMatch(Kind kind)
      : kind_(kind)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    switch (kind_) {
    case Kind::File:
      return !item.is_directory;
    case Kind::Dir:
      return item.is_directory;
    case Kind::Any:
      return true;
    }
    return false;
  }

private:
  Kind kind_;
};

class SizeMatch : public MatchFunc {
public:
  enum class Op { Eq, Ne, Lt, Le, Gt, Ge, Range };
  SizeMatch(Op op, std::uint64_t a, std::uint64_t b = 0)
      : op_(op)
      , a_(a)
      , b_(b)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    if (item.is_directory) {
      return false;
    }
    const auto s = item.size;
    switch (op_) {
    case Op::Eq:
      return s == a_;
    case Op::Ne:
      return s != a_;
    case Op::Lt:
      return s < a_;
    case Op::Le:
      return s <= a_;
    case Op::Gt:
      return s > a_;
    case Op::Ge:
      return s >= a_;
    case Op::Range:
      return s >= a_ && s <= b_;
    }
    return false;
  }

private:
  Op op_;
  std::uint64_t a_;
  std::uint64_t b_;
};

} // namespace

MatchFuncPtr make_name_substring(std::string needle, bool case_sensitive)
{
  return std::make_shared<NameSubstringMatch>(std::move(needle), case_sensitive);
}

MatchFuncPtr make_glob(std::string pattern, bool case_sensitive)
{
  return std::make_shared<GlobMatch>(std::move(pattern), case_sensitive);
}

MatchFuncPtr make_regex(std::string pattern, bool case_sensitive)
{
  try {
    const auto flags =
        case_sensitive ? std::regex::ECMAScript
                       : (std::regex::ECMAScript | std::regex::icase);
    return std::make_shared<RegexMatch>(std::regex(pattern, flags));
  } catch (const std::regex_error&) {
    return std::make_shared<AlwaysFalse>();
  }
}

MatchFuncPtr make_type(std::string argument)
{
  const auto a = to_lower(std::move(argument));
  if (a == "file" || a == "f" || a == "regular") {
    return std::make_shared<TypeMatch>(TypeMatch::Kind::File);
  }
  if (a == "dir" || a == "directory" || a == "folder" || a == "d") {
    return std::make_shared<TypeMatch>(TypeMatch::Kind::Dir);
  }
  return std::make_shared<AlwaysFalse>();
}

MatchFuncPtr make_size(std::string argument)
{
  std::string arg = argument;
  // Trim spaces
  while (!arg.empty() && std::isspace(static_cast<unsigned char>(arg.front()))) {
    arg.erase(arg.begin());
  }
  while (!arg.empty() && std::isspace(static_cast<unsigned char>(arg.back()))) {
    arg.pop_back();
  }
  if (arg.empty()) {
    return std::make_shared<AlwaysFalse>();
  }

  // Range: 10K-2M
  if (const auto dash = arg.find('-'); dash != std::string::npos && dash > 0
      && arg.find_first_of("<>=") == std::string::npos) {
    const auto lo = parse_size_token(arg.substr(0, dash));
    const auto hi = parse_size_token(arg.substr(dash + 1));
    if (lo && hi) {
      return std::make_shared<SizeMatch>(SizeMatch::Op::Range, *lo, *hi);
    }
  }

  SizeMatch::Op op = SizeMatch::Op::Eq;
  std::string_view rest = arg;
  if (arg.starts_with(">=")) {
    op = SizeMatch::Op::Ge;
    rest = std::string_view{arg}.substr(2);
  } else if (arg.starts_with("<=")) {
    op = SizeMatch::Op::Le;
    rest = std::string_view{arg}.substr(2);
  } else if (arg.starts_with("!=") || arg.starts_with("<>")) {
    op = SizeMatch::Op::Ne;
    rest = std::string_view{arg}.substr(2);
  } else if (arg.starts_with(">")) {
    op = SizeMatch::Op::Gt;
    rest = std::string_view{arg}.substr(1);
  } else if (arg.starts_with("<")) {
    op = SizeMatch::Op::Lt;
    rest = std::string_view{arg}.substr(1);
  } else if (arg.starts_with("=")) {
    op = SizeMatch::Op::Eq;
    rest = std::string_view{arg}.substr(1);
  }

  while (!rest.empty() && std::isspace(static_cast<unsigned char>(rest.front()))) {
    rest.remove_prefix(1);
  }
  const auto val = parse_size_token(rest);
  if (!val) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<SizeMatch>(op, *val);
}


namespace {

enum class Cmp { Eq, Ne, Lt, Le, Gt, Ge };

std::pair<Cmp, std::string_view> split_cmp(std::string_view arg)
{
  while (!arg.empty() && std::isspace(static_cast<unsigned char>(arg.front()))) {
    arg.remove_prefix(1);
  }
  if (arg.starts_with(">=")) {
    return {Cmp::Ge, arg.substr(2)};
  }
  if (arg.starts_with("<=")) {
    return {Cmp::Le, arg.substr(2)};
  }
  if (arg.starts_with("!=") || arg.starts_with("<>")) {
    return {Cmp::Ne, arg.substr(2)};
  }
  if (arg.starts_with(">")) {
    return {Cmp::Gt, arg.substr(1)};
  }
  if (arg.starts_with("<")) {
    return {Cmp::Lt, arg.substr(1)};
  }
  if (arg.starts_with("=")) {
    return {Cmp::Eq, arg.substr(1)};
  }
  return {Cmp::Eq, arg};
}

bool apply_cmp(Cmp op, double a, double b)
{
  switch (op) {
  case Cmp::Eq:
    return a == b;
  case Cmp::Ne:
    return a != b;
  case Cmp::Lt:
    return a < b;
  case Cmp::Le:
    return a <= b;
  case Cmp::Gt:
    return a > b;
  case Cmp::Ge:
    return a >= b;
  }
  return false;
}


std::optional<MediaInfo> lookup_media(const std::filesystem::path& path)
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

class WidthMatch : public MatchFunc {
public:
  WidthMatch(Cmp op, double value)
      : op_(op)
      , value_(value)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    if (item.is_directory || item.path.empty()) {
      return false;
    }
    const auto meta = lookup_media(item.path);
    if (!meta || !meta->width) {
      return false;
    }
    return apply_cmp(op_, static_cast<double>(*meta->width), value_);
  }

private:
  Cmp op_;
  double value_;
};

class HeightMatch : public MatchFunc {
public:
  HeightMatch(Cmp op, double value)
      : op_(op)
      , value_(value)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    if (item.is_directory || item.path.empty()) {
      return false;
    }
    const auto meta = lookup_media(item.path);
    if (!meta || !meta->height) {
      return false;
    }
    return apply_cmp(op_, static_cast<double>(*meta->height), value_);
  }

private:
  Cmp op_;
  double value_;
};

class DurationMatch : public MatchFunc {
public:
  DurationMatch(Cmp op, double seconds)
      : op_(op)
      , ms_(seconds * 1000.0)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    if (item.is_directory || item.path.empty()) {
      return false;
    }
    const auto meta = lookup_media(item.path);
    if (!meta || !meta->duration_ms) {
      return false;
    }
    return apply_cmp(op_, static_cast<double>(*meta->duration_ms), ms_);
  }

private:
  Cmp op_;
  double ms_;
};

class FramerateMatch : public MatchFunc {
public:
  FramerateMatch(Cmp op, double value)
      : op_(op)
      , value_(value)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    if (item.is_directory || item.path.empty()) {
      return false;
    }
    const auto meta = lookup_media(item.path);
    if (!meta || !meta->framerate) {
      return false;
    }
    return apply_cmp(op_, *meta->framerate, value_);
  }

private:
  Cmp op_;
  double value_;
};

std::optional<double> parse_number_arg(std::string_view rest)
{
  while (!rest.empty() && std::isspace(static_cast<unsigned char>(rest.front()))) {
    rest.remove_prefix(1);
  }
  while (!rest.empty() && std::isspace(static_cast<unsigned char>(rest.back()))) {
    rest.remove_suffix(1);
  }
  if (rest.empty()) {
    return std::nullopt;
  }
  try {
    return std::stod(std::string{rest});
  } catch (...) {
    return std::nullopt;
  }
}

} // namespace

MatchFuncPtr make_width(std::string argument)
{
  const auto [op, rest] = split_cmp(argument);
  const auto val = parse_number_arg(rest);
  if (!val) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<WidthMatch>(op, *val);
}

MatchFuncPtr make_height(std::string argument)
{
  const auto [op, rest] = split_cmp(argument);
  const auto val = parse_number_arg(rest);
  if (!val) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<HeightMatch>(op, *val);
}

MatchFuncPtr make_duration(std::string argument)
{
  const auto [op, rest] = split_cmp(argument);
  const auto secs = parse_duration_seconds(rest);
  if (!secs) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<DurationMatch>(op, *secs);
}

MatchFuncPtr make_framerate(std::string argument)
{
  const auto [op, rest] = split_cmp(argument);
  const auto val = parse_number_arg(rest);
  if (!val) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<FramerateMatch>(op, *val);
}



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

  // Split on '@' from the right for optional params
  auto take_suffix_num = [&](double& out_d, int* out_i) -> bool {
    const auto at = needle.rfind('@');
    if (at == std::string::npos || at == 0) {
      return false;
    }
    const std::string tail = needle.substr(at + 1);
    try {
      if (out_i != nullptr) {
        *out_i = std::stoi(tail);
      } else {
        out_d = std::stod(tail);
      }
      needle.resize(at);
      return true;
    } catch (...) {
      return false;
    }
  };

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



namespace {

std::string lower_copy(std::string s)
{
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

std::optional<std::int64_t> resolve_mtime_sec(const FilterItem& item)
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

std::pair<LenCmp, std::string_view> split_len_cmp(std::string_view arg)
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

bool apply_len_cmp(LenCmp op, double a, double b)
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

class LengthMatch : public MatchFunc {
public:
  LengthMatch(LenCmp op, std::size_t value)
      : op_(op)
      , value_(value)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    return apply_len_cmp(op_, static_cast<double>(item.name.size()), static_cast<double>(value_));
  }

private:
  LenCmp op_;
  std::size_t value_;
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

} // namespace

MatchFuncPtr make_length(std::string argument)
{
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

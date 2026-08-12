// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/predicates.hpp"

#include "dirtoo/filter/media_meta_cache.hpp"
#include "predicates_detail.hpp"

#include <algorithm>
#include <cmath>
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




class NumericRangeMatch : public MatchFunc {
public:
  enum class Kind { Width, Height, Framerate };
  NumericRangeMatch(Kind kind, double lo, double hi)
      : kind_(kind)
      , lo_(std::min(lo, hi))
      , hi_(std::max(lo, hi))
  {
  }
  bool matches(const FilterItem& item) const override
  {
    if (item.is_directory || item.path.empty()) {
      return false;
    }
    const auto meta = detail::lookup_media(item.path);
    if (!meta) {
      return false;
    }
    std::optional<double> v;
    switch (kind_) {
    case Kind::Width:
      if (meta->width) {
        v = static_cast<double>(*meta->width);
      }
      break;
    case Kind::Height:
      if (meta->height) {
        v = static_cast<double>(*meta->height);
      }
      break;
    case Kind::Framerate:
      v = meta->framerate;
      break;
    }
    if (!v) {
      return false;
    }
    return *v >= lo_ && *v <= hi_;
  }

private:
  Kind kind_;
  double lo_;
  double hi_;
};

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
    const auto meta = detail::lookup_media(item.path);
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
    const auto meta = detail::lookup_media(item.path);
    if (!meta || !meta->height) {
      return false;
    }
    return apply_cmp(op_, static_cast<double>(*meta->height), value_);
  }

private:
  Cmp op_;
  double value_;
};

class AspectMatch : public MatchFunc {
public:
  AspectMatch(Cmp op, double ratio)
      : op_(op)
      , ratio_(ratio)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    if (item.is_directory || item.path.empty()) {
      return false;
    }
    const auto meta = detail::lookup_media(item.path);
    if (!meta || !meta->width || !meta->height || *meta->height == 0) {
      return false;
    }
    const double ar = static_cast<double>(*meta->width) / static_cast<double>(*meta->height);
    if (op_ == Cmp::Eq) {
      // Tolerate small probe rounding (e.g. 16:9 ≈ 1.777…).
      return std::abs(ar - ratio_) < 0.02;
    }
    if (op_ == Cmp::Ne) {
      return std::abs(ar - ratio_) >= 0.02;
    }
    return apply_cmp(op_, ar, ratio_);
  }

private:
  Cmp op_;
  double ratio_;
};

class DurationMatch : public MatchFunc {
public:
  DurationMatch(Cmp op, double seconds)
      : range_(false)
      , op_(op)
      , lo_ms_(seconds * 1000.0)
      , hi_ms_(0)
  {
  }
  /// Inclusive range in seconds.
  DurationMatch(double lo_seconds, double hi_seconds)
      : range_(true)
      , op_(Cmp::Eq)
      , lo_ms_(lo_seconds * 1000.0)
      , hi_ms_(hi_seconds * 1000.0)
  {
    if (lo_ms_ > hi_ms_) {
      std::swap(lo_ms_, hi_ms_);
    }
  }
  bool matches(const FilterItem& item) const override
  {
    if (item.is_directory || item.path.empty()) {
      return false;
    }
    const auto meta = detail::lookup_media(item.path);
    if (!meta || !meta->duration_ms) {
      return false;
    }
    const double ms = static_cast<double>(*meta->duration_ms);
    if (range_) {
      return ms >= lo_ms_ && ms <= hi_ms_;
    }
    return apply_cmp(op_, ms, lo_ms_);
  }

private:
  bool range_;
  Cmp op_;
  double lo_ms_;
  double hi_ms_;
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
    const auto meta = detail::lookup_media(item.path);
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
  if (const auto rng = detail::split_range_arg(argument)) {
    const auto lo = parse_number_arg(rng->first);
    const auto hi = parse_number_arg(rng->second);
    if (lo && hi) {
      return std::make_shared<NumericRangeMatch>(NumericRangeMatch::Kind::Width, *lo, *hi);
    }
    return std::make_shared<AlwaysFalse>();
  }
  const auto [op, rest] = split_cmp(argument);
  const auto val = parse_number_arg(rest);
  if (!val) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<WidthMatch>(op, *val);
}

MatchFuncPtr make_height(std::string argument)
{
  if (const auto rng = detail::split_range_arg(argument)) {
    const auto lo = parse_number_arg(rng->first);
    const auto hi = parse_number_arg(rng->second);
    if (lo && hi) {
      return std::make_shared<NumericRangeMatch>(NumericRangeMatch::Kind::Height, *lo, *hi);
    }
    return std::make_shared<AlwaysFalse>();
  }
  const auto [op, rest] = split_cmp(argument);
  const auto val = parse_number_arg(rest);
  if (!val) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<HeightMatch>(op, *val);
}

namespace {

std::optional<double> parse_aspect_value(std::string_view rest)
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
  const auto colon = rest.find(':');
  if (colon != std::string_view::npos) {
    const auto left = parse_number_arg(rest.substr(0, colon));
    const auto right = parse_number_arg(rest.substr(colon + 1));
    if (!left || !right || *right == 0.0) {
      return std::nullopt;
    }
    return *left / *right;
  }
  return parse_number_arg(rest);
}

} // namespace

MatchFuncPtr make_aspect(std::string argument)
{
  const auto [op, rest] = split_cmp(argument);
  const auto val = parse_aspect_value(rest);
  if (!val) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<AspectMatch>(op, *val);
}

namespace {

/// If @p lo is a bare number and @p hi ends with a single duration unit (h/m/s),
/// treat lo as having the same unit so duration:3-10m means 3–10 minutes.
std::optional<double> parse_duration_lo_with_inherited_unit(std::string_view lo, std::string_view hi)
{
  auto lo_secs = parse_duration_seconds(lo);
  if (!lo_secs) {
    return std::nullopt;
  }
  // Pure number on the left?
  bool pure = !lo.empty();
  for (char c : lo) {
    if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.')) {
      pure = false;
      break;
    }
  }
  if (!pure) {
    return lo_secs;
  }
  // Single trailing unit on hi (not colon times, not compound 1h2m).
  if (hi.find(':') != std::string_view::npos) {
    return lo_secs;
  }
  std::size_t unit_chars = 0;
  for (char c : hi) {
    const char l = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (l == 'h' || l == 'm' || l == 's') {
      ++unit_chars;
    }
  }
  if (unit_chars != 1) {
    return lo_secs;
  }
  char unit = 0;
  for (std::size_t i = hi.size(); i > 0; --i) {
    const char l = static_cast<char>(std::tolower(static_cast<unsigned char>(hi[i - 1])));
    if (l == 'h' || l == 'm' || l == 's') {
      unit = l;
      break;
    }
  }
  if (unit == 0) {
    return lo_secs;
  }
  std::string combined{lo};
  combined.push_back(unit);
  return parse_duration_seconds(combined);
}

} // namespace

MatchFuncPtr make_duration(std::string argument)
{
  // Inclusive range: duration:3-10m / duration:3m..10m / duration:90-180
  if (const auto rng = detail::split_range_arg(argument)) {
    const auto lo = parse_duration_lo_with_inherited_unit(rng->first, rng->second);
    const auto hi = parse_duration_seconds(rng->second);
    if (lo && hi) {
      return std::make_shared<DurationMatch>(*lo, *hi);
    }
    return std::make_shared<AlwaysFalse>();
  }
  const auto [op, rest] = split_cmp(argument);
  const auto secs = parse_duration_seconds(rest);
  if (!secs) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<DurationMatch>(op, *secs);
}

MatchFuncPtr make_framerate(std::string argument)
{
  if (const auto rng = detail::split_range_arg(argument)) {
    const auto lo = parse_number_arg(rng->first);
    const auto hi = parse_number_arg(rng->second);
    if (lo && hi) {
      return std::make_shared<NumericRangeMatch>(NumericRangeMatch::Kind::Framerate, *lo, *hi);
    }
    return std::make_shared<AlwaysFalse>();
  }
  const auto [op, rest] = split_cmp(argument);
  const auto val = parse_number_arg(rest);
  if (!val) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<FramerateMatch>(op, *val);
}

} // namespace dirtoo::filter

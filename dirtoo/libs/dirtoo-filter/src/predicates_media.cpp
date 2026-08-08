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
      : op_(op)
      , ms_(seconds * 1000.0)
  {
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

} // namespace dirtoo::filter

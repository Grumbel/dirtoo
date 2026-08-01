// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/parser.hpp"
#include "dirtoo/filter/filter_item.hpp"

#include <catch2/catch_test_macros.hpp>

using dirtoo::filter::FilterItem;
using dirtoo::filter::parse_filter;

static FilterItem file(const char* name, std::uint64_t size = 0)
{
  return FilterItem{.name = name, .size = size, .is_directory = false, .path = name};
}

static FilterItem dir(const char* name)
{
  return FilterItem{.name = name, .size = 0, .is_directory = true, .path = name};
}

TEST_CASE("filter substring and glob", "[filter]")
{
  auto m = parse_filter("png");
  REQUIRE(m);
  REQUIRE((*m)->matches(file("photo.png")));
  REQUIRE_FALSE((*m)->matches(file("photo.jpg")));

  m = parse_filter("*.jpg");
  REQUIRE(m);
  REQUIRE((*m)->matches(file("a.jpg")));
  REQUIRE_FALSE((*m)->matches(file("a.png")));
}

TEST_CASE("filter parentheses and OR", "[filter]")
{
  auto m = parse_filter("(readme OR license) -*.bak");
  REQUIRE(m);
  REQUIRE((*m)->matches(file("README")));
  REQUIRE((*m)->matches(file("license.txt")));
  REQUIRE_FALSE((*m)->matches(file("readme.bak")));
  REQUIRE_FALSE((*m)->matches(file("other")));
}

TEST_CASE("filter AND juxtaposition and size", "[filter]")
{
  auto m = parse_filter("type:file size:>100");
  REQUIRE(m);
  REQUIRE((*m)->matches(file("big.bin", 200)));
  REQUIRE_FALSE((*m)->matches(file("small.bin", 50)));
  REQUIRE_FALSE((*m)->matches(dir("folder")));
}

TEST_CASE("filter nested groups", "[filter]")
{
  auto m = parse_filter("(a OR b) AND (c OR d)");
  REQUIRE(m);
  REQUIRE((*m)->matches(file("ac")));
  REQUIRE((*m)->matches(file("bd")));
  REQUIRE_FALSE((*m)->matches(file("ab")));
  REQUIRE_FALSE((*m)->matches(file("cd")));
}

TEST_CASE("filter unbalanced paren", "[filter]")
{
  auto m = parse_filter("(a OR b");
  REQUIRE_FALSE(m);
}

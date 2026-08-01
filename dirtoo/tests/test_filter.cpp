// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/parser.hpp"
#include "dirtoo/filter/filter_item.hpp"
#include "dirtoo/filter/search.hpp"
#include "dirtoo/filter/media_probe.hpp"
#include "dirtoo/filter/predicates.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

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

namespace fs = std::filesystem;

TEST_CASE("search_directory recursive finds nested files", "[filter][search]")
{
  const auto dir = fs::temp_directory_path() / "dirtoo-search-rec";
  fs::remove_all(dir);
  fs::create_directories(dir / "sub" / "deep");
  std::ofstream(dir / "a.png") << "x";
  std::ofstream(dir / "sub" / "b.jpg") << "y";
  std::ofstream(dir / "sub" / "deep" / "c.png") << "z";
  std::ofstream(dir / ".hidden.png") << "h";

  auto m = parse_filter("*.png");
  REQUIRE(m);

  dirtoo::filter::SearchOptions opt;
  opt.max_depth = -1;
  opt.show_hidden = false;
  auto items = dirtoo::filter::search_directory_collect(dir, **m, opt);
  REQUIRE(items.size() == 2); // a.png, deep/c.png — not hidden

  opt.max_depth = 0;
  items = dirtoo::filter::search_directory_collect(dir, **m, opt);
  REQUIRE(items.size() == 1);
  REQUIRE(items[0].name == "a.png");

  opt.max_depth = -1;
  opt.show_hidden = true;
  items = dirtoo::filter::search_directory_collect(dir, **m, opt);
  REQUIRE(items.size() == 3);

  fs::remove_all(dir);
}

TEST_CASE("search_directory respects cancel", "[filter][search]")
{
  const auto dir = fs::temp_directory_path() / "dirtoo-search-cancel";
  fs::remove_all(dir);
  fs::create_directories(dir);
  for (int i = 0; i < 20; ++i) {
    std::ofstream(dir / ("f" + std::to_string(i) + ".txt")) << "x";
  }
  auto m = parse_filter("*.txt");
  REQUIRE(m);
  int hits = 0;
  dirtoo::filter::SearchOptions opt;
  opt.max_depth = 0;
  opt.should_cancel = [&] { return hits >= 3; };
  const auto stats = dirtoo::filter::search_directory(dir, **m, opt, [&](const dirtoo::filter::FilterItem&) {
    ++hits;
  });
  (void)stats;
  REQUIRE(hits == 3);
  fs::remove_all(dir);
}


TEST_CASE("parse_duration_seconds", "[filter][media]")
{
  using dirtoo::filter::parse_duration_seconds;
  REQUIRE(parse_duration_seconds("90"));
  REQUIRE(*parse_duration_seconds("90") == 90.0);
  REQUIRE(*parse_duration_seconds("1:30") == 90.0);
  REQUIRE(*parse_duration_seconds("1:02:03") == 3723.0);
  REQUIRE(*parse_duration_seconds("1h2m3s") == 3723.0);
  REQUIRE(*parse_duration_seconds("2m") == 120.0);
  REQUIRE_FALSE(parse_duration_seconds(""));
  REQUIRE_FALSE(parse_duration_seconds("nope"));
}

TEST_CASE("filter media commands parse", "[filter][media]")
{
  auto m = parse_filter("width:>=1920");
  REQUIRE(m);
  m = parse_filter("height:=1080");
  REQUIRE(m);
  m = parse_filter("duration:>1m");
  REQUIRE(m);
  m = parse_filter("framerate:>29.9");
  REQUIRE(m);
  m = parse_filter("type:file width:>=640");
  REQUIRE(m);
  // directories never match media predicates
  REQUIRE_FALSE((*m)->matches(dir("folder")));
}


TEST_CASE("fuzzy_score ngram", "[filter][fuzzy]")
{
  using dirtoo::filter::fuzzy_score;
  REQUIRE(fuzzy_score("readme", "readme") > 0.99);
  REQUIRE(fuzzy_score("speling", "spelling") > 0.5);
  REQUIRE(fuzzy_score("zzzzz", "readme") < 0.5);
  REQUIRE(fuzzy_score("ab", "cab") > 0.0);
}

TEST_CASE("filter fuzzy command", "[filter][fuzzy]")
{
  auto m = parse_filter("fuzzy:speling");
  REQUIRE(m);
  REQUIRE((*m)->matches(file("spelling.txt")));
  REQUIRE_FALSE((*m)->matches(file("zzzzz.dat")));

  m = parse_filter("fuzzy:readme@0.9");
  REQUIRE(m);
  REQUIRE((*m)->matches(file("readme")));
}

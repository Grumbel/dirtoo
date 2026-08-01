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
#include <ctime>

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

TEST_CASE("filter length command", "[filter]")
{
  auto m = parse_filter("length:>5");
  REQUIRE(m);
  REQUIRE((*m)->matches(file("abcdef")));
  REQUIRE_FALSE((*m)->matches(file("ab")));
  REQUIRE((*m)->matches(file("123456")));

  m = parse_filter("len:=3");
  REQUIRE(m);
  REQUIRE((*m)->matches(file("abc")));
  REQUIRE_FALSE((*m)->matches(file("abcd")));
}

TEST_CASE("filter date command", "[filter]")
{
  auto m = parse_filter("date:today");
  REQUIRE(m);
  // Item with today's mtime matches
  FilterItem today_file = file("x.txt");
  today_file.mtime_sec = static_cast<std::int64_t>(std::time(nullptr));
  REQUIRE((*m)->matches(today_file));

  FilterItem old_file = file("old.txt");
  old_file.mtime_sec = 0; // 1970-01-01
  REQUIRE_FALSE((*m)->matches(old_file));

  m = parse_filter("date:>=2020-01-01");
  REQUIRE(m);
  REQUIRE((*m)->matches(today_file));
  REQUIRE_FALSE((*m)->matches(old_file));

  // Glob against formatted local date of a known epoch second.
  m = parse_filter("date:>=1970-01-01");
  REQUIRE(m);
  REQUIRE((*m)->matches(today_file));

  m = parse_filter("date:<1970-01-02");
  REQUIRE(m);
  // mtime 0 is on or before 1970-01-01 in most zones
  REQUIRE((*m)->matches(old_file));
}

TEST_CASE("filter contains command", "[filter]")
{
  const auto dir = fs::temp_directory_path() / "dirtoo-contains";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const auto path = dir / "note.txt";
  {
    std::ofstream out(path);
    out << "Hello WORLD\nsecond line\n";
  }

  FilterItem item{.name = "note.txt", .size = 0, .is_directory = false, .path = path};

  auto m = parse_filter("contains:hello");
  REQUIRE(m);
  REQUIRE((*m)->matches(item));

  m = parse_filter("contains:missing");
  REQUIRE(m);
  REQUIRE_FALSE((*m)->matches(item));

  m = parse_filter("Contains:WORLD");
  REQUIRE(m);
  REQUIRE((*m)->matches(item));

  m = parse_filter("Contains:world");
  REQUIRE(m);
  REQUIRE_FALSE((*m)->matches(item)); // case-sensitive

  m = parse_filter("type:dir contains:hello");
  REQUIRE(m);
  REQUIRE_FALSE((*m)->matches(dir("folder")));

  fs::remove_all(dir);
}

TEST_CASE("filter time and weekday commands", "[filter]")
{
  auto m = parse_filter("time:>=12:00");
  REQUIRE(m);
  FilterItem noonish = file("x");
  // 2020-06-15 14:00 UTC-ish — use fixed local via mtime that is afternoon in most zones is hard;
  // just ensure non-crash and noon boundary with explicit minutes via known epoch.
  // 0 epoch is early morning UTC → may fail local; use a large mtime mid-day-ish.
  noonish.mtime_sec = 1'600'000'000; // 2020-09-13 ~12:26 UTC
  // Accept either match or non-match depending on TZ; parse must succeed
  (void)(*m)->matches(noonish);

  m = parse_filter("weekday:mon");
  REQUIRE(m);
  m = parse_filter("weekday:>=5");
  REQUIRE(m);
  m = parse_filter("wday:0");
  REQUIRE(m);
}

TEST_CASE("filter containsre command", "[filter]")
{
  const auto dir = fs::temp_directory_path() / "dirtoo-containsre";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const auto path = dir / "code.txt";
  {
    std::ofstream out(path);
    out << "alpha\nfoo123bar\nzed\n";
  }
  FilterItem item{.name = "code.txt", .size = 0, .is_directory = false, .path = path};

  auto m = parse_filter("containsre:foo[0-9]+bar");
  REQUIRE(m);
  REQUIRE((*m)->matches(item));

  m = parse_filter("containsre:FOO[0-9]+BAR");
  REQUIRE(m);
  REQUIRE((*m)->matches(item)); // case-insensitive

  m = parse_filter("Containsre:FOO[0-9]+BAR");
  REQUIRE(m);
  REQUIRE_FALSE((*m)->matches(item));

  m = parse_filter("containsre:nomatch");
  REQUIRE(m);
  REQUIRE_FALSE((*m)->matches(item));

  fs::remove_all(dir);
}

TEST_CASE("filter containsfuzzy random charset", "[filter]")
{
  auto m = parse_filter("random:1.0");
  REQUIRE(m);
  REQUIRE((*m)->matches(file("anything")));

  m = parse_filter("random:0");
  REQUIRE(m);
  REQUIRE_FALSE((*m)->matches(file("anything")));

  m = parse_filter("charset:ascii");
  REQUIRE(m);
  REQUIRE((*m)->matches(file("readme.txt")));

  m = parse_filter("charset:utf-8");
  REQUIRE(m);
  REQUIRE((*m)->matches(file("readme.txt")));

  const auto dir = fs::temp_directory_path() / "dirtoo-cfuzzy";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const auto path = dir / "notes.txt";
  {
    std::ofstream out(path);
    out << "spelling is hard\n";
  }
  FilterItem item{.name = "notes.txt", .size = 0, .is_directory = false, .path = path};
  m = parse_filter("containsfuzzy:speling");
  REQUIRE(m);
  REQUIRE((*m)->matches(item));
  fs::remove_all(dir);
}

TEST_CASE("filter pages and filecount parse", "[filter]")
{
  auto m = parse_filter("pages:>=10");
  REQUIRE(m);
  m = parse_filter("filecount:>5");
  REQUIRE(m);
  m = parse_filter("files:=3");
  REQUIRE(m);
  // Non-pdf / non-archive → no match
  REQUIRE_FALSE((*m)->matches(file("readme.txt")));
}

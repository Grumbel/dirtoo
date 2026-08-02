// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/archive/archive_index.hpp"
#include "dirtoo/fs/location.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("fileinfos_for_prefix immediate children", "[archive]")
{
  using dirtoo::archive::ArchiveEntry;

  std::vector<ArchiveEntry> entries;
  entries.push_back({.path = "readme.txt", .is_directory = false, .size = 10});
  entries.push_back({.path = "docs", .is_directory = true, .size = 0});
  entries.push_back({.path = "docs/a.md", .is_directory = false, .size = 3});
  entries.push_back({.path = "docs/b.md", .is_directory = false, .size = 4});
  entries.push_back({.path = "src/main.cpp", .is_directory = false, .size = 100});

  const auto root = dirtoo::fs::Location::from_archive("/tmp/demo.zip", "");
  const auto items = dirtoo::archive::fileinfos_for_prefix(root, entries);
  REQUIRE(items.size() == 3); // readme.txt, docs, src

  const auto docs = dirtoo::fs::Location::from_archive("/tmp/demo.zip", "docs");
  const auto under_docs = dirtoo::archive::fileinfos_for_prefix(docs, entries);
  REQUIRE(under_docs.size() == 2);
  REQUIRE(under_docs[0].basename() != under_docs[1].basename());
}

TEST_CASE("parse_tv_listing_text extracts sizes", "[archive]")
{
  // GNU/BSD-ish: mode links user group size mon day time name
  const std::string sample =
      "-rw-r--r--  1 user group  1234 Jan  1 12:00 readme.txt\n"
      "drwxr-xr-x  1 user group     0 Jan  1 12:00 docs\n"
      "-rw-r--r--  1 user group    99 Jan  1 12:00 docs/a.md\n";
  const auto entries = dirtoo::archive::parse_tv_listing_text(sample);
  REQUIRE(entries.size() >= 2);
  bool found_readme = false;
  for (const auto& e : entries) {
    if (e.path == "readme.txt") {
      found_readme = true;
      REQUIRE(e.size == 1234);
      REQUIRE_FALSE(e.is_directory);
    }
    if (e.path == "docs") {
      REQUIRE(e.is_directory);
    }
  }
  REQUIRE(found_readme);
}

TEST_CASE("parse_unzip_listing_text extracts sizes", "[archive]")
{
  const std::string sample =
      "Archive:  foo.zip\n"
      "  Length      Date    Time    Name\n"
      "---------  ---------- -----   ----\n"
      "     1234  2020-01-01 12:00   readme.txt\n"
      "        0  2020-01-01 12:00   docs/\n"
      "       99  2020-01-01 12:00   docs/a.md\n"
      "---------                     -------\n"
      "     1333                     3 files\n";
  const auto entries = dirtoo::archive::parse_unzip_listing_text(sample);
  REQUIRE(entries.size() >= 2);
  bool found = false;
  for (const auto& e : entries) {
    if (e.path == "readme.txt") {
      found = true;
      REQUIRE(e.size == 1234);
    }
  }
  REQUIRE(found);
}

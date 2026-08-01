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

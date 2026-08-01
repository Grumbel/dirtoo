// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/collection/file_collection.hpp"
#include "dirtoo/collection/sorter.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/fs/location.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("FileCollection hides dotfiles by default", "[collection]")
{
  const auto dir = fs::temp_directory_path() / "dirtoo-collection-hidden";
  fs::remove_all(dir);
  fs::create_directories(dir);
  std::ofstream(dir / "visible.txt") << "x";
  std::ofstream(dir / ".secret") << "y";

  dirtoo::collection::FileCollection col;
  auto items = dirtoo::fs::list_directory(dirtoo::fs::Location::from_path(dir));
  col.set_items(std::move(items));

  REQUIRE(col.visible_items().size() == 1);
  REQUIRE(col.visible_items().front().basename() == "visible.txt");

  col.set_show_hidden(true);
  REQUIRE(col.visible_items().size() == 2);

  col.set_name_filter("sec");
  REQUIRE(col.visible_items().size() == 1);
  REQUIRE(col.visible_items().front().basename() == ".secret");

  fs::remove_all(dir);
}

TEST_CASE("FileCollection glob filter", "[collection]")
{
  const auto dir = fs::temp_directory_path() / "dirtoo-collection-glob";
  fs::remove_all(dir);
  fs::create_directories(dir);
  std::ofstream(dir / "a.png") << "x";
  std::ofstream(dir / "b.jpg") << "y";
  std::ofstream(dir / "readme.txt") << "z";

  dirtoo::collection::FileCollection col;
  col.set_items(dirtoo::fs::list_directory(dirtoo::fs::Location::from_path(dir)));
  col.set_name_filter("*.png");
  REQUIRE(col.visible_items().size() == 1);
  REQUIRE(col.visible_items().front().basename() == "a.png");

  col.set_name_filter("*.jpg");
  REQUIRE(col.visible_items().size() == 1);

  col.set_name_filter("read");
  REQUIRE(col.visible_items().size() == 1);
  REQUIRE(col.visible_items().front().basename() == "readme.txt");

  fs::remove_all(dir);
}


TEST_CASE("numeric_sort_key natural order", "[collection][sort]")
{
  using dirtoo::collection::numeric_sort_key;
  using dirtoo::collection::Sorter;
  using dirtoo::collection::SortKey;

  // file2 before file10
  REQUIRE(Sorter{}.compare(
      dirtoo::fs::FileInfo::synthetic(dirtoo::fs::Location::from_path("/t/file2"), "file2", false),
      dirtoo::fs::FileInfo::synthetic(dirtoo::fs::Location::from_path("/t/file10"), "file10", false)) < 0);

  dirtoo::collection::FileCollection col;
  std::vector<dirtoo::fs::FileInfo> items;
  items.push_back(dirtoo::fs::FileInfo::synthetic(dirtoo::fs::Location::from_path("/t/file10"), "file10", false));
  items.push_back(dirtoo::fs::FileInfo::synthetic(dirtoo::fs::Location::from_path("/t/file2"), "file2", false));
  items.push_back(dirtoo::fs::FileInfo::synthetic(dirtoo::fs::Location::from_path("/t/dir"), "dir", true));
  col.set_items(std::move(items));
  col.set_directories_first(true);
  col.sort_by_name(true);
  REQUIRE(col.visible_items().front().basename() == "dir");
  REQUIRE(col.visible_items()[1].basename() == "file2");
  REQUIRE(col.visible_items()[2].basename() == "file10");
}

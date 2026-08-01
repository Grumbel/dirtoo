// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/collection/file_collection.hpp"
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

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

TEST_CASE("FileCollection group by day", "[collection][group]")
{
  using dirtoo::collection::FileCollection;
  using dirtoo::collection::GroupMode;
  using dirtoo::fs::FileInfo;
  using dirtoo::fs::Location;

  FileCollection col;
  std::vector<FileInfo> items;
  items.push_back(FileInfo::synthetic(Location::from_path("/t/a"), "a.txt", false));
  items.push_back(FileInfo::synthetic(Location::from_path("/t/b"), "b.txt", false));
  items.push_back(FileInfo::synthetic(Location::from_path("/t/dir"), "dir", true));
  col.set_items(std::move(items));
  col.set_group_mode(GroupMode::Day);
  // directories are ungrouped; labels for synthetics may be Unknown date
  REQUIRE(col.group_mode() == GroupMode::Day);
  REQUIRE(col.visible_items().size() == 3);
  // directory has empty day label
  bool found_dir_empty = false;
  for (const auto& fi : col.visible_items()) {
    if (fi.is_directory()) {
      REQUIRE(col.group_label_for(fi).empty());
      found_dir_empty = true;
    }
  }
  REQUIRE(found_dir_empty);
}

TEST_CASE("FileCollection group by directory", "[collection][group]")
{
  using dirtoo::collection::FileCollection;
  using dirtoo::collection::GroupMode;
  using dirtoo::fs::FileInfo;
  using dirtoo::fs::Location;

  FileCollection col;
  std::vector<FileInfo> items;
  items.push_back(FileInfo::synthetic(Location::from_path("/t/x/a"), "a", false));
  items.push_back(FileInfo::synthetic(Location::from_path("/t/y/b"), "b", false));
  items.push_back(FileInfo::synthetic(Location::from_path("/t/x/c"), "c", false));
  col.set_items(std::move(items));
  col.set_group_mode(GroupMode::Directory);
  REQUIRE(col.visible_items().size() == 3);
  // Contiguous groups after stable_sort by key
  const auto& v = col.visible_items();
  const auto l0 = col.group_label_for(v[0]);
  const auto l1 = col.group_label_for(v[1]);
  const auto l2 = col.group_label_for(v[2]);
  // Two share /t/x, one /t/y — middle should match one of the ends after grouping
  const int same01 = (l0 == l1) ? 1 : 0;
  const int same12 = (l1 == l2) ? 1 : 0;
  REQUIRE(same01 + same12 == 1);
}

TEST_CASE("group_key duration buckets", "[collection][group]")
{
  using dirtoo::collection::GroupMode;
  using dirtoo::collection::group_key;
  using dirtoo::collection::group_label;
  using dirtoo::fs::FileInfo;
  using dirtoo::fs::Location;

  auto dir = FileInfo::synthetic(Location::from_path("/t/d"), "d", true);
  REQUIRE(group_key(dir, GroupMode::Duration) == "9");
  REQUIRE(group_label(dir, GroupMode::Duration) == "Directories");

  auto file = FileInfo::synthetic(Location::from_path("/t/v.mp4"), "v.mp4", false);
  // No media cache → unknown
  REQUIRE(group_key(file, GroupMode::Duration) == "8");
  REQUIRE(group_label(file, GroupMode::Duration) == "Unknown duration");
}

TEST_CASE("FileCollection merge_items add remove update", "[collection]")
{
  using dirtoo::collection::FileCollection;
  using dirtoo::fs::FileInfo;
  using dirtoo::fs::Location;

  FileCollection col;
  std::vector<FileInfo> initial;
  initial.push_back(FileInfo::synthetic(Location::from_path("/t/a"), "a", false));
  initial.push_back(FileInfo::synthetic(Location::from_path("/t/b"), "b", false));
  initial.push_back(FileInfo::synthetic(Location::from_path("/t/c"), "c", false));
  col.set_items_unsorted(std::move(initial));
  REQUIRE(col.size() == 3);

  // Drop b, keep a/c order, append d.
  std::vector<FileInfo> next;
  next.push_back(FileInfo::synthetic(Location::from_path("/t/a"), "a", false));
  next.push_back(FileInfo::synthetic(Location::from_path("/t/c"), "c", false));
  next.push_back(FileInfo::synthetic(Location::from_path("/t/d"), "d", false));
  col.merge_items(std::move(next));

  REQUIRE(col.size() == 3);
  REQUIRE(col.items()[0].basename() == "a");
  REQUIRE(col.items()[1].basename() == "c");
  REQUIRE(col.items()[2].basename() == "d");

  // merge without rebuild leaves size correct
  std::vector<FileInfo> only_a;
  only_a.push_back(FileInfo::synthetic(Location::from_path("/t/a"), "a", false));
  col.merge_items(std::move(only_a), false);
  REQUIRE(col.size() == 1);
}


TEST_CASE("FileCollection group by session gaps", "[collection][group]")
{
  using dirtoo::collection::FileCollection;
  using dirtoo::collection::GroupMode;
  using dirtoo::collection::kSessionGapThreshold;
  using dirtoo::fs::FileInfo;
  using dirtoo::fs::Location;

  // Without controllable mtime on synthetic entries, verify mode switches and
  // that apply_grouping returns same-size label vector.
  std::vector<FileInfo> items;
  items.push_back(FileInfo::synthetic(Location::from_path("/t/a"), "a", false));
  items.push_back(FileInfo::synthetic(Location::from_path("/t/b"), "b", false));
  items.push_back(FileInfo::synthetic(Location::from_path("/t/c"), "c", false));

  auto labels = dirtoo::collection::apply_grouping(items, GroupMode::Session);
  REQUIRE(labels.size() == 3);
  // All synthetic → unknown session label, single group.
  REQUIRE(labels[0] == labels[1]);
  REQUIRE(labels[1] == labels[2]);
  REQUIRE_FALSE(labels[0].empty());

  FileCollection col;
  col.set_items(std::move(items));
  col.set_group_mode(GroupMode::Session);
  REQUIRE(col.group_mode() == GroupMode::Session);
  REQUIRE(col.visible_items().size() == 3);
  REQUIRE(col.is_group_start_at(0));
  REQUIRE_FALSE(col.is_group_start_at(1));
  (void)kSessionGapThreshold;
}

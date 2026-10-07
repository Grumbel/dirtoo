// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/collection/file_collection.hpp"
#include "dirtoo/collection/sorter.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/fs/location.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fcntl.h>
#include <sys/stat.h>

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

namespace {

std::vector<std::string> sorted_names(std::vector<std::string> names, bool ascending = true,
                                      dirtoo::collection::SortKey key =
                                          dirtoo::collection::SortKey::Name)
{
  const auto dir = fs::temp_directory_path() / "dirtoo-collection-sorted";
  fs::remove_all(dir);
  fs::create_directories(dir);
  for (const auto& n : names) {
    std::ofstream(dir / n) << n;  // size = name length
  }
  auto items = dirtoo::fs::list_directory(dirtoo::fs::Location::from_path(dir));
  dirtoo::collection::Sorter sorter;
  sorter.set_key(key);
  sorter.set_ascending(ascending);
  sorter.sort(items);
  std::vector<std::string> out;
  for (const auto& fi : items) {
    out.push_back(fi.basename());
  }
  fs::remove_all(dir);
  return out;
}

} // namespace

TEST_CASE("natural sort orders long digit runs numerically", "[collection][sort]")
{
  // 2^64 + 1 wraps to 1 in a uint64 and used to sort before "f2".
  const auto out = sorted_names({"f18446744073709551617.txt", "f3.txt", "f2.txt",
                                 "f99999999999999999999999.txt", "f10.txt"});
  const std::vector<std::string> want = {"f2.txt", "f3.txt", "f10.txt",
                                         "f18446744073709551617.txt",
                                         "f99999999999999999999999.txt"};
  CHECK(out == want);
}

TEST_CASE("natural sort ignores leading zeros for order but stays deterministic", "[collection][sort]")
{
  const auto a = sorted_names({"x007", "x7", "x10", "x08"});
  // 7 == 007 numerically; both before 08 (8) and 10.
  REQUIRE(a.size() == 4);
  CHECK(a[2] == "x08");
  CHECK(a[3] == "x10");
  CHECK(((a[0] == "x007" && a[1] == "x7") || (a[0] == "x7" && a[1] == "x007")));
  // Same input, same order every time.
  CHECK(sorted_names({"x007", "x7", "x10", "x08"}) == a);
}

TEST_CASE("sort descending and by size keep name tie-breaks", "[collection][sort]")
{
  using dirtoo::collection::SortKey;
  // "a","b","c" have equal size 1; "dd" is 2.
  CHECK(sorted_names({"c", "dd", "a", "b"}, true, SortKey::Size)
        == std::vector<std::string>{"a", "b", "c", "dd"});
  CHECK(sorted_names({"c", "dd", "a", "b"}, false, SortKey::Size)
        == std::vector<std::string>{"dd", "c", "b", "a"});
  CHECK(sorted_names({"b10", "b2", "B1"}, false)
        == std::vector<std::string>{"b10", "b2", "B1"});
}

TEST_CASE("random sort is stable until reshuffled", "[collection][sort]")
{
  const auto dir = fs::temp_directory_path() / "dirtoo-collection-random";
  fs::remove_all(dir);
  fs::create_directories(dir / "adir");
  fs::create_directories(dir / "zdir");
  for (int i = 0; i < 40; ++i) {
    std::ofstream(dir / ("f" + std::to_string(i))) << "x";
  }
  auto items = dirtoo::fs::list_directory(dirtoo::fs::Location::from_path(dir));

  dirtoo::collection::Sorter sorter;
  sorter.set_key(dirtoo::collection::SortKey::Random);
  auto names = [](const std::vector<dirtoo::fs::FileInfo>& v) {
    std::vector<std::string> out;
    for (const auto& fi : v) {
      out.push_back(fi.basename());
    }
    return out;
  };

  auto first = items;
  sorter.sort(first);
  // Same seed, any input order -> same result (a watcher refresh hands in a
  // differently ordered list).
  auto reversed = items;
  std::reverse(reversed.begin(), reversed.end());
  sorter.sort(reversed);
  CHECK(names(first) == names(reversed));

  // Removing one entry does not reshuffle the others.
  auto fewer = first;
  fewer.erase(fewer.begin() + 10);
  sorter.sort(fewer);
  auto expected = names(first);
  expected.erase(expected.begin() + 10);
  CHECK(names(fewer) == expected);

  // Directories still come first.
  CHECK(first[0].is_directory());
  CHECK(first[1].is_directory());

  // A new seed deals a different order.
  sorter.set_random_seed(1);
  auto other = items;
  sorter.sort(other);
  sorter.set_random_seed(2);
  auto third = items;
  sorter.sort(third);
  CHECK(names(other) != names(third));
  fs::remove_all(dir);
}

TEST_CASE("sort by modified uses sub-second times and puts unknown times first", "[collection][sort]")
{
  const auto dir = fs::temp_directory_path() / "dirtoo-collection-mtime-sort";
  fs::remove_all(dir);
  fs::create_directories(dir);
  // Same second; names are in the opposite order of the times.
  const timespec older[2] = {{1577934245, 100000000}, {1577934245, 100000000}};
  const timespec newer[2] = {{1577934245, 900000000}, {1577934245, 900000000}};
  std::ofstream(dir / "z_old") << "x";
  std::ofstream(dir / "a_new") << "x";
  REQUIRE(::utimensat(AT_FDCWD, (dir / "z_old").c_str(), older, 0) == 0);
  REQUIRE(::utimensat(AT_FDCWD, (dir / "a_new").c_str(), newer, 0) == 0);

  auto items = dirtoo::fs::list_directory(dirtoo::fs::Location::from_path(dir));
  items.push_back(dirtoo::fs::FileInfo::synthetic(
      dirtoo::fs::Location::from_archive("/tmp/x.zip", "member"), "member", false, 1));

  dirtoo::collection::Sorter sorter;
  sorter.set_key(dirtoo::collection::SortKey::Modified);
  sorter.set_directories_first(false);
  sorter.sort(items);
  REQUIRE(items.size() == 3);
  CHECK(items[0].basename() == "member");  // no time: before all real times, not "year 2174"
  CHECK(items[1].basename() == "z_old");
  CHECK(items[2].basename() == "a_new");
  fs::remove_all(dir);
}

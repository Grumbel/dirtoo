// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Watcher-driven updates must not reorder the listing: modified entries
// change in place, new ones are appended, and the collection reports when the
// result no longer matches a fresh sort/filter ("stale").

#include "live_update_queue.hpp"

#include "dirtoo/collection/file_collection.hpp"
#include "dirtoo/collection/sorter.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/fs/location.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <chrono>
#include <string>
#include <vector>

using dirtoo::collection::FileCollection;
using dirtoo::collection::SortKey;
using dirtoo::fs::FileInfo;
using dirtoo::fs::Location;

namespace {

FileInfo entry(const std::string& name, std::uint64_t size)
{
  return FileInfo::synthetic(Location::from_path("/virtual/live/" + name), name, false, size);
}

std::vector<std::string> visible_names(const FileCollection& col)
{
  std::vector<std::string> names;
  for (const auto& fi : col.visible_items()) {
    names.push_back(fi.basename());
  }
  return names;
}

FileCollection sized_collection()
{
  FileCollection col;
  col.sorter().set_key(SortKey::Size);
  col.set_items({entry("a", 10), entry("b", 20), entry("c", 30)});
  return col;
}

} // namespace

TEST_CASE("Sorter::is_sorted / in_order follow sort()", "[collection][live]")
{
  dirtoo::collection::Sorter sorter;
  sorter.set_key(SortKey::Size);
  std::vector<FileInfo> items{entry("x", 3), entry("y", 1), entry("z", 2)};
  REQUIRE_FALSE(sorter.is_sorted(items));
  sorter.sort(items);
  REQUIRE(sorter.is_sorted(items));
  REQUIRE(sorter.in_order(items[0], items[1]));
  REQUIRE_FALSE(sorter.in_order(items[2], items[0]));

  sorter.set_ascending(false);
  REQUIRE_FALSE(sorter.is_sorted(items));
  sorter.sort(items);
  REQUIRE(sorter.is_sorted(items));

  sorter.set_key(SortKey::Random);
  sorter.sort(items);
  REQUIRE(sorter.is_sorted(items));
}

TEST_CASE("upsert_in_place updates a grown file without moving it", "[collection][live]")
{
  auto col = sized_collection();
  // "a" grows past everything: a fresh sort would move it to the end.
  const auto update = col.upsert_in_place({entry("a", 99)});
  REQUIRE(visible_names(col) == std::vector<std::string>{"a", "b", "c"});
  REQUIRE(col.visible_items()[0].size() == 99);
  REQUIRE(update.changed_rows == std::vector<std::size_t>{0});
  REQUIRE(update.appended_rows == 0);
  REQUIRE(update.added_paths.empty());
  REQUIRE(update.stale);

  // Growth that keeps the order is not stale.
  auto col2 = sized_collection();
  REQUIRE_FALSE(col2.upsert_in_place({entry("b", 25)}).stale);
}

TEST_CASE("upsert_in_place appends new entries at the end", "[collection][live]")
{
  auto col = sized_collection();
  auto update = col.upsert_in_place({entry("new", 1)});
  REQUIRE(visible_names(col) == std::vector<std::string>{"a", "b", "c", "new"});
  REQUIRE(update.appended_rows == 1);
  REQUIRE(update.added_paths == std::vector<std::string>{"/virtual/live/new"});
  REQUIRE(update.stale);  // size 1 belongs at the top

  update = col.upsert_in_place({entry("big", 1000)});
  REQUIRE(update.appended_rows == 1);

  // Hidden newcomers are listed but not shown.
  update = col.upsert_in_place({entry(".part", 5)});
  REQUIRE(update.appended_rows == 0);
  REQUIRE(update.added_paths.size() == 1);
  REQUIRE(col.items().size() == 6);
  REQUIRE(col.visible_items().size() == 5);
}

TEST_CASE("upsert_in_place never changes filter membership of known entries", "[collection][live]")
{
  auto col = sized_collection();
  col.set_name_filter("size:>15");
  REQUIRE(visible_names(col) == std::vector<std::string>{"b", "c"});

  // "b" shrinks below the filter: it stays visible until the user reloads.
  auto update = col.upsert_in_place({entry("b", 1)});
  REQUIRE(visible_names(col) == std::vector<std::string>{"b", "c"});
  REQUIRE(update.stale);

  // "a" grows into the filter: not shown either, but stale.
  update = col.upsert_in_place({entry("a", 50)});
  REQUIRE(visible_names(col) == std::vector<std::string>{"b", "c"});
  REQUIRE(update.changed_rows.empty());
  REQUIRE(update.stale);
}

TEST_CASE("erase_in_place keeps the order of the rest", "[collection][live]")
{
  auto col = sized_collection();
  col.upsert_in_place({entry("a", 99)});
  REQUIRE(col.erase_in_place(Location::from_path("/virtual/live/b")));
  REQUIRE(visible_names(col) == std::vector<std::string>{"a", "c"});
  REQUIRE(col.items().size() == 2);
  REQUIRE_FALSE(col.erase_in_place(Location::from_path("/virtual/live/b")));
}

TEST_CASE("replace_visible_keep_order keeps shown rows in place", "[collection][live]")
{
  auto col = sized_collection();
  col.upsert_in_place({entry("a", 99)});  // visible: a b c (unsorted now)
  // A refilter result arrives sorted, plus a newcomer.
  std::vector<FileInfo> filtered{entry("n", 5), entry("b", 20), entry("c", 30), entry("a", 99)};
  REQUIRE_FALSE(col.replace_visible_keep_order(std::move(filtered)));
  REQUIRE(visible_names(col) == std::vector<std::string>{"a", "b", "c", "n"});
}

TEST_CASE("LiveUpdateQueue throttles modify-only bursts and merges paths", "[live]")
{
  int argc = 0;
  QCoreApplication app(argc, nullptr);
  using namespace std::chrono_literals;
  dirtoo::app::LiveUpdateQueue queue;
  queue.set_intervals(20ms, 300ms);
  int ready = 0;
  QObject::connect(&queue, &dirtoo::app::LiveUpdateQueue::ready, &queue, [&] { ++ready; });
  const auto pump = [](int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
      QThread::msleep(2);
    }
  };

  // A file written continuously: one delivery per modify interval, not per write.
  for (int i = 0; i < 10; ++i) {
    queue.add({}, {}, {QStringLiteral("/d/download")});
    pump(10);
  }
  REQUIRE(ready == 0);
  pump(400);
  REQUIRE(ready == 1);
  auto batch = queue.take();
  REQUIRE(batch.upserts == QStringList{QStringLiteral("/d/download")});
  REQUIRE(batch.removed.isEmpty());

  // A creation shortens the wait.
  queue.add({}, {}, {QStringLiteral("/d/download")});
  queue.add({QStringLiteral("/d/new")}, {}, {});
  pump(100);
  REQUIRE(ready == 2);
  batch = queue.take();
  REQUIRE(batch.upserts == (QStringList{QStringLiteral("/d/download"), QStringLiteral("/d/new")}));

  // Removed and created in one burst: stat decides (upsert); a later removal wins.
  queue.add({QStringLiteral("/d/tmp")}, {QStringLiteral("/d/tmp")}, {});
  batch = queue.take();
  REQUIRE(batch.upserts == QStringList{QStringLiteral("/d/tmp")});
  queue.add({QStringLiteral("/d/tmp")}, {}, {});
  queue.add({}, {QStringLiteral("/d/tmp")}, {});
  batch = queue.take();
  REQUIRE(batch.upserts.isEmpty());
  REQUIRE(batch.removed == QStringList{QStringLiteral("/d/tmp")});
}

TEST_CASE("LiveUpdateQueue drops and counts changes while paused", "[live]")
{
  int argc = 0;
  QCoreApplication app(argc, nullptr);
  using namespace std::chrono_literals;
  dirtoo::app::LiveUpdateQueue queue;
  queue.set_intervals(10ms, 10ms);
  int ready = 0;
  QObject::connect(&queue, &dirtoo::app::LiveUpdateQueue::ready, &queue, [&] { ++ready; });

  queue.add({QStringLiteral("/d/queued")}, {}, {});
  queue.set_paused(true);
  REQUIRE(queue.missed() == 1);  // the queued change was not applied
  queue.add({}, {}, {QStringLiteral("/d/a"), QStringLiteral("/d/b")});
  queue.add_rescan();
  REQUIRE(queue.missed() == 4);
  QElapsedTimer t;
  t.start();
  while (t.elapsed() < 50) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
  }
  REQUIRE(ready == 0);
  REQUIRE(queue.take().empty());

  queue.set_paused(false);
  queue.reset_missed();
  REQUIRE(queue.missed() == 0);
}

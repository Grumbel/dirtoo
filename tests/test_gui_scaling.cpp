// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Headless (QT_QPA_PLATFORM=offscreen) checks that the file views do not do
// work proportional to the *whole* directory for per-item or per-frame events.
// The thresholds are deliberately loose (10-100x margin) so slow CI machines do
// not flake, yet an O(n) loop per event at this size cannot meet them.

#include "file_list_model.hpp"
#include "graphics_file_view.hpp"

#include "dirtoo/collection/file_collection.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/fs/location.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QString>

#include <algorithm>
#include <string>
#include <vector>

using dirtoo::app::FileListModel;
using dirtoo::app::GraphicsFileView;

namespace {

constexpr int kSmall = 2000;
constexpr int kLarge = 200000;

std::vector<dirtoo::fs::FileInfo> make_items(int n)
{
  std::vector<dirtoo::fs::FileInfo> items;
  items.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    const std::string name = "file_" + std::to_string(i) + ".txt";
    items.push_back(dirtoo::fs::FileInfo::synthetic(
        dirtoo::fs::Location::from_path("/virtual/dir/" + name), name, false,
        static_cast<std::uint64_t>(i)));
  }
  return items;
}

struct App {
  int argc = 1;
  char name[8] = "tests";
  char* argv[2] = {name, nullptr};
  QApplication app{argc, argv};
};


/// Milliseconds for 300 thumbnail results arriving for the *last* rows of a model
/// with @p rows entries (the worst case for a linear search).
qint64 thumbnail_update_ms(int rows)
{
  dirtoo::collection::FileCollection col;
  col.set_items_unsorted(make_items(rows));
  FileListModel model;
  model.set_collection(&col);
  REQUIRE(model.rowCount() == rows);

  QElapsedTimer timer;
  timer.start();
  for (int i = 0; i < 300; ++i) {
    const QString path = QStringLiteral("/virtual/dir/file_%1.txt").arg(rows - 1 - i);
    model.set_thumbnail_failed(path, QStringLiteral("test"));
  }
  return timer.elapsed();
}

/// Milliseconds for 20 repaints of a Graphics view over @p rows entries.
qint64 repaint_ms(int rows)
{
  dirtoo::collection::FileCollection col;
  col.set_items_unsorted(make_items(rows));
  FileListModel model;
  model.set_collection(&col);

  GraphicsFileView view;
  view.resize(900, 600);
  view.set_model(&model);
  view.show();
  QApplication::processEvents();
  view.viewport()->grab();  // warm up (creates the visible tiles)

  QElapsedTimer timer;
  timer.start();
  for (int i = 0; i < 20; ++i) {
    view.viewport()->grab();
  }
  return timer.elapsed();
}

} // namespace

// Relative checks: the same work on a small and on a 100x larger directory must
// cost about the same. (A linear scan per event is ~100x slower on the large one.)

TEST_CASE("per-thumbnail model updates do not scan the whole directory", "[gui][scaling]")
{
  App a;
  const auto small = thumbnail_update_ms(kSmall);
  const auto large = thumbnail_update_ms(kLarge);
  WARN("300 thumbnail updates: " << small << " ms @ " << kSmall << " rows, " << large << " ms @ "
                                 << kLarge << " rows");
  // One O(n) index build is allowed (200000 rows: a few tens of ms); 300
  // linear scans (~2 s) are not.
  CHECK(large < 25 * small + 150);
}

TEST_CASE("painting the Graphics view costs the visible window, not the directory", "[gui][scaling]")
{
  App a;
  const auto small = repaint_ms(kSmall);
  const auto large = repaint_ms(kLarge);
  WARN("20 repaints: " << small << " ms @ " << kSmall << " rows, " << large << " ms @ " << kLarge
                       << " rows");
  CHECK(large < 2 * small + 40);
}

TEST_CASE("path to row lookup follows reordering and filtering", "[gui][model]")
{
  App a;
  dirtoo::collection::FileCollection col;
  col.set_items_unsorted(make_items(50));
  FileListModel model;
  model.set_collection(&col);

  std::vector<int> changed_rows;
  QObject::connect(&model, &QAbstractItemModel::dataChanged, &model,
                   [&](const QModelIndex& tl, const QModelIndex&) { changed_rows.push_back(tl.row()); });
  auto path_of_row = [&](int r) {
    return QString::fromStdString(col.visible_items()[static_cast<std::size_t>(r)].path().string());
  };

  // Initial lookup.
  const QString p7 = path_of_row(7);
  model.set_thumbnail_failed(p7);
  REQUIRE_FALSE(changed_rows.empty());
  CHECK(changed_rows.back() == 7);

  // Reverse the order, announced to the model: the same path is now another row.
  {
    auto items = col.items();
    std::reverse(items.begin(), items.end());
    col.set_items_unsorted(std::move(items));
    model.refresh();
  }
  changed_rows.clear();
  model.set_thumbnail_pending(p7);
  REQUIRE_FALSE(changed_rows.empty());
  CHECK(changed_rows.back() == 42);  // 49 - 7

  // Reorder WITHOUT telling the model: the lookup verifies and still finds the right row.
  {
    auto items = col.items();
    std::reverse(items.begin(), items.end());
    col.set_items_unsorted(std::move(items));
  }
  changed_rows.clear();
  model.set_thumbnail_failed(p7);
  REQUIRE_FALSE(changed_rows.empty());
  CHECK(path_of_row(changed_rows.back()) == p7);

  // A path that is not (or no longer) visible changes nothing.
  changed_rows.clear();
  model.set_thumbnail_failed(QStringLiteral("/virtual/dir/does-not-exist"));
  CHECK(changed_rows.empty());
}

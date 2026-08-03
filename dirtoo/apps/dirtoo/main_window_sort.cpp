// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include <QHeaderView>
#include "dirtoo/collection/file_collection.hpp"

namespace dirtoo::app {

void MainWindow::request_async_sort()
{
  // Content filters own visible_ via FilterWorker. Sorting must not call
  // replace_items_sorted → rebuild_visible (GUI content I/O / wipe filter).
  if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()
      && filter_expression_needs_content_io(filter_edit_->text())) {
    collection_.sort_items_only();
    request_async_filter(/*keep_previous_visible=*/true);
    return;
  }
  if (list_workers_.sort() == nullptr) {
    collection_.apply_sort();
    refresh_list();
    return;
  }
  const quint64 gen = list_workers_.next_sort_generation();
  auto items = collection_.items(); // copy
  const auto key = collection_.sorter().key();
  const bool asc = collection_.sorter().ascending();
  const bool dirs_first = collection_.sorter().directories_first();
  QMetaObject::invokeMethod(list_workers_.sort(), "sort_items", Qt::QueuedConnection,
                            Q_ARG(std::vector<dirtoo::fs::FileInfo>, items),
                            Q_ARG(dirtoo::collection::SortKey, key), Q_ARG(bool, asc),
                            Q_ARG(bool, dirs_first), Q_ARG(quint64, gen));
}

void MainWindow::on_sort_finished(quint64 generation, std::vector<fs::FileInfo> items)
{
  if (generation != list_workers_.sort_generation() || search_active_) {
    return;
  }
  // match_/filter and show_hidden stay; only item order changes.
  collection_.replace_items_sorted(std::move(items));
  refresh_list();
  set_status(QStringLiteral("%1 items").arg(collection_.visible_items().size()));
  request_thumbnails_for_visible();
}

void MainWindow::apply_sort_key(collection::SortKey key, bool toggle_if_same)
{
  if (toggle_if_same && sort_key_ == key) {
    sort_ascending_ = !sort_ascending_;
  } else {
    sort_key_ = key;
    sort_ascending_ = true;
  }
  collection_.sorter().set_key(key);
  collection_.sorter().set_ascending(sort_ascending_);
  update_sort_toolbar_label();
  if (tree_view_ != nullptr && tree_view_->header() != nullptr) {
    tree_view_->header()->setSortIndicatorShown(true);
    // Map SortKey → visible FileListColumn for indicator when possible.
    int section = static_cast<int>(FileListColumn::Name);
    switch (key) {
    case collection::SortKey::Size:
      section = static_cast<int>(FileListColumn::Size);
      break;
    case collection::SortKey::Width:
      section = static_cast<int>(FileListColumn::Width);
      break;
    case collection::SortKey::Height:
      section = static_cast<int>(FileListColumn::Height);
      break;
    case collection::SortKey::Resolution:
      section = static_cast<int>(FileListColumn::Dimensions);
      break;
    case collection::SortKey::AspectRatio:
      section = static_cast<int>(FileListColumn::AspectRatio);
      break;
    case collection::SortKey::Framerate:
      section = static_cast<int>(FileListColumn::Framerate);
      break;
    case collection::SortKey::Duration:
      section = static_cast<int>(FileListColumn::Duration);
      break;
    case collection::SortKey::Modified:
      section = static_cast<int>(FileListColumn::Modified);
      break;
    case collection::SortKey::Type:
    case collection::SortKey::Extension:
      section = static_cast<int>(FileListColumn::Type);
      break;
    default:
      section = static_cast<int>(FileListColumn::Name);
      break;
    }
    tree_view_->header()->setSortIndicator(
        section, sort_ascending_ ? Qt::AscendingOrder : Qt::DescendingOrder);
  }
  request_async_sort();
}

void MainWindow::update_sort_toolbar_label()
{
  if (sort_toolbar_btn_ == nullptr) {
    return;
  }
  QString title = QStringLiteral("Name");
  switch (sort_key_) {
  case collection::SortKey::Size:
    title = QStringLiteral("Size");
    break;
  case collection::SortKey::Modified:
    title = QStringLiteral("Modified");
    break;
  case collection::SortKey::Type:
  case collection::SortKey::Extension:
    title = QStringLiteral("Type");
    break;
  case collection::SortKey::Width:
    title = QStringLiteral("Width");
    break;
  case collection::SortKey::Height:
    title = QStringLiteral("Height");
    break;
  case collection::SortKey::Resolution:
    title = QStringLiteral("Dimensions");
    break;
  case collection::SortKey::AspectRatio:
    title = QStringLiteral("Aspect");
    break;
  case collection::SortKey::Duration:
    title = QStringLiteral("Duration");
    break;
  case collection::SortKey::Framerate:
    title = QStringLiteral("FPS");
    break;
  case collection::SortKey::Random:
    title = QStringLiteral("Random");
    break;
  default:
    title = QStringLiteral("Name");
    break;
  }
  sort_toolbar_btn_->setText(title);
}

void MainWindow::on_header_clicked(int section)
{
  using collection::SortKey;
  SortKey key = SortKey::Name;
  switch (static_cast<FileListColumn>(section)) {
  case FileListColumn::Name:
    key = SortKey::Name;
    break;
  case FileListColumn::Size:
    key = SortKey::Size;
    break;
  case FileListColumn::Width:
    key = SortKey::Width;
    break;
  case FileListColumn::Height:
    key = SortKey::Height;
    break;
  case FileListColumn::Dimensions:
    key = SortKey::Resolution; // width * height
    break;
  case FileListColumn::AspectRatio:
    key = SortKey::AspectRatio;
    break;
  case FileListColumn::Framerate:
    key = SortKey::Framerate;
    break;
  case FileListColumn::Duration:
    key = SortKey::Duration;
    break;
  case FileListColumn::Modified:
  case FileListColumn::Accessed:
  case FileListColumn::Changed:
  case FileListColumn::Birth:
    // Extra time columns share Modified sort until dedicated SortKeys exist.
    key = SortKey::Modified;
    break;
  case FileListColumn::Type:
    key = SortKey::Type;
    break;
  case FileListColumn::Count:
    return;
  }
  apply_sort_key(key, /*toggle_if_same=*/true);
}

} // namespace dirtoo::app

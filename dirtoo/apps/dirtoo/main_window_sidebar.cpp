// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include <QList>
#include <filesystem>

namespace dirtoo::app {

void MainWindow::on_toggle_sidebar(bool checked)
{
  if (sidebar_widget_ != nullptr) {
    sidebar_widget_->setVisible(checked);
  }
  if (main_splitter_ != nullptr && checked && sidebar_widget_ != nullptr) {
    QList<int> sizes = main_splitter_->sizes();
    if (sizes.size() >= 2 && sizes[0] < 80) {
      sizes[0] = 220;
      main_splitter_->setSizes(sizes);
    }
  }
}

void MainWindow::on_sidebar_activated(const QModelIndex& index)
{
  if (!index.isValid()) {
    return;
  }
  const QString path = sidebar_places_.path_for_index(index);
  if (path.isEmpty()) {
    return;
  }
  open_location(fs::Location::from_path(std::filesystem::path(path.toStdString())), true);
}

void MainWindow::sync_sidebar_to_location()
{
  sidebar_places_.sync_to_location(sidebar_tree_, sidebar_widget_, location_);
}

void MainWindow::rebuild_sidebar_places()
{
  sidebar_places_.rebuild(bookmarks_);
}

} // namespace dirtoo::app

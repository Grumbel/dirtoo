// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include <filesystem>

namespace dirtoo::app {

void MainWindow::on_toggle_sidebar(bool checked)
{
  sidebar_.toggle(checked);
}

void MainWindow::on_sidebar_activated(const QModelIndex& index)
{
  sidebar_.on_activated(index);
}

void MainWindow::sync_sidebar_to_location()
{
  sidebar_.sync_to_location(location_);
}

void MainWindow::rebuild_sidebar_places()
{
  sidebar_.rebuild(bookmarks_);
}

} // namespace dirtoo::app

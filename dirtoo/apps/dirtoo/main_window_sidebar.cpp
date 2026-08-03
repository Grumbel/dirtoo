// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QStandardPaths>
#include <QStringList>
#include <filesystem>

namespace dirtoo::app {

void MainWindow::on_toggle_sidebar(bool checked)
{
  if (sidebar_widget_ != nullptr) {
    sidebar_widget_->setVisible(checked);
  }
  if (main_splitter_ != nullptr && checked && sidebar_widget_ != nullptr) {
    // Restore a usable width if collapsed
    QList<int> sizes = main_splitter_->sizes();
    if (sizes.size() >= 2 && sizes[0] < 80) {
      sizes[0] = 220;
      main_splitter_->setSizes(sizes);
    }
  }
}

void MainWindow::on_sidebar_activated(const QModelIndex& index)
{
  if (!index.isValid() || directory_tree_model_ == nullptr) {
    return;
  }
  const QString path = directory_tree_model_->path_for_index(index);
  if (path.isEmpty()) {
    return;
  }
  open_location(fs::Location::from_path(std::filesystem::path(path.toStdString())), true);
}

void MainWindow::sync_sidebar_to_location()
{
  if (sidebar_tree_ == nullptr || directory_tree_model_ == nullptr || sidebar_widget_ == nullptr
      || !sidebar_widget_->isVisible()) {
    return;
  }
  if (!location_.is_file()) {
    return;
  }
  const QString path = QString::fromStdString(location_.as_path().string());
  const QModelIndex ix = directory_tree_model_->ensure_path_visible(path);
  if (!ix.isValid()) {
    return;
  }
  for (QModelIndex parent = ix.parent(); parent.isValid(); parent = parent.parent()) {
    sidebar_tree_->expand(parent);
  }
  sidebar_tree_->expand(ix);
  sidebar_tree_->setCurrentIndex(ix);
  sidebar_tree_->scrollTo(ix, QAbstractItemView::PositionAtCenter);
}


void MainWindow::rebuild_sidebar_places()
{
  if (directory_tree_model_ == nullptr) {
    return;
  }
  QStringList roots;
  QStringList labels;
  const QString home = QDir::homePath();
  roots << home;
  labels << QStringLiteral("Home");
  roots << QStringLiteral("/");
  labels << QStringLiteral("Filesystem");
  for (auto loc : {QStandardPaths::DesktopLocation, QStandardPaths::DocumentsLocation,
                   QStandardPaths::DownloadLocation, QStandardPaths::MusicLocation,
                   QStandardPaths::PicturesLocation, QStandardPaths::MoviesLocation}) {
    const QString path = QStandardPaths::writableLocation(loc);
    if (!path.isEmpty() && QDir(path).exists() && path != home && !roots.contains(path)) {
      roots << path;
      labels << QStandardPaths::displayName(loc);
    }
  }
  for (const auto& loc : bookmarks_.entries()) {
    if (!loc.is_file()) {
      continue;
    }
    const QString path = QString::fromStdString(loc.as_path().string());
    if (path.isEmpty() || roots.contains(path)) {
      continue;
    }
    roots << path;
    labels << QFileInfo(path).fileName();
  }
  directory_tree_model_->reset_roots(roots, labels);
}



} // namespace dirtoo::app

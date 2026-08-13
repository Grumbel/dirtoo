// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sidebar_places.hpp"

#include <QAbstractItemView>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>

namespace dirtoo::app {

SidebarPlaces::SidebarPlaces(QObject* parent)
    : QObject(parent)
{
}

DirectoryTreeModel* SidebarPlaces::ensure_model()
{
  if (model_ == nullptr) {
    model_ = new DirectoryTreeModel(this);
  }
  return model_;
}

void SidebarPlaces::rebuild(const Bookmarks& bookmarks)
{
  auto* m = ensure_model();
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

  // User bookmarks under a non-interactive "Bookmarks" heading so they are not
  // mixed with the standard places list.
  QStringList bookmark_paths;
  QStringList bookmark_labels;
  for (const auto& loc : bookmarks.entries()) {
    if (!loc.is_file()) {
      continue;
    }
    const QString path = QString::fromStdString(loc.as_path().string());
    if (path.isEmpty() || roots.contains(path) || bookmark_paths.contains(path)) {
      continue;
    }
    bookmark_paths << path;
    bookmark_labels << QFileInfo(path).fileName();
  }
  if (!bookmark_paths.isEmpty()) {
    roots << QString(); // section header (empty path)
    labels << QStringLiteral("Bookmarks");
    roots << bookmark_paths;
    labels << bookmark_labels;
  }

  m->reset_roots(roots, labels);
}

void SidebarPlaces::set_show_hidden(bool show)
{
  if (model_ != nullptr) {
    model_->set_show_hidden(show);
  }
}

void SidebarPlaces::sync_to_location(QTreeView* tree, QWidget* sidebar_host,
                                    const fs::Location& location)
{
  if (tree == nullptr || model_ == nullptr || sidebar_host == nullptr || !sidebar_host->isVisible()) {
    return;
  }
  if (!location.is_file()) {
    return;
  }
  const QString path = QString::fromStdString(location.as_path().string());
  const QModelIndex ix = model_->ensure_path_visible(path);
  if (!ix.isValid()) {
    return;
  }
  for (QModelIndex parent = ix.parent(); parent.isValid(); parent = parent.parent()) {
    tree->expand(parent);
  }
  tree->expand(ix);
  tree->setCurrentIndex(ix);
  tree->scrollTo(ix, QAbstractItemView::PositionAtCenter);
}

QString SidebarPlaces::path_for_index(const QModelIndex& index) const
{
  if (model_ == nullptr || !index.isValid()) {
    return {};
  }
  return model_->path_for_index(index);
}

} // namespace dirtoo::app

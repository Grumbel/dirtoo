// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "bookmarks.hpp"
#include "directory_tree_model.hpp"

#include "dirtoo/fs/location.hpp"

#include <QObject>
#include <QString>
#include <QTreeView>
#include <QWidget>

namespace dirtoo::app {

/// Owns the places DirectoryTreeModel and implements rebuild/sync helpers.
class SidebarPlaces : public QObject {
  Q_OBJECT
public:
  explicit SidebarPlaces(QObject* parent = nullptr);

  [[nodiscard]] DirectoryTreeModel* model() const { return model_; }
  /// Create model if needed (parent is this).
  DirectoryTreeModel* ensure_model();

  void rebuild(const Bookmarks& bookmarks);
  void set_show_hidden(bool show);
  void sync_to_location(QTreeView* tree, QWidget* sidebar_host, const fs::Location& location);

  [[nodiscard]] QString path_for_index(const QModelIndex& index) const;

private:
  DirectoryTreeModel* model_ = nullptr;
};

} // namespace dirtoo::app

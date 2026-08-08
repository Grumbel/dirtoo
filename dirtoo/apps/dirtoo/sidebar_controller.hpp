// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "bookmarks.hpp"
#include "sidebar_places.hpp"

#include "dirtoo/fs/location.hpp"

#include <QObject>
#include <QSplitter>
#include <QString>
#include <QTreeView>
#include <QWidget>
#include <functional>

namespace dirtoo::app {

/// Directory-tree / places sidebar: visibility, activation, rebuild, sync.
class SidebarController : public QObject {
  Q_OBJECT
public:
  explicit SidebarController(QObject* parent = nullptr);

  void bind(QWidget* host, QTreeView* tree, QSplitter* main_splitter);

  [[nodiscard]] SidebarPlaces& places() { return places_; }
  [[nodiscard]] const SidebarPlaces& places() const { return places_; }
  [[nodiscard]] QWidget* widget() const { return widget_; }
  [[nodiscard]] QTreeView* tree() const { return tree_; }

  void set_visible(bool visible);
  void toggle(bool checked);
  void rebuild(const Bookmarks& bookmarks);
  void sync_to_location(const fs::Location& location);
  void set_show_hidden(bool show);

  /// Path for a places index, or empty.
  [[nodiscard]] QString path_for_index(const QModelIndex& index) const;

  /// Optional: open a filesystem path (used when a places row is activated).
  void set_open_path_handler(std::function<void(const QString& path)> handler);

public slots:
  void on_activated(const QModelIndex& index);

private:
  SidebarPlaces places_{this};
  QWidget* widget_ = nullptr;
  QTreeView* tree_ = nullptr;
  QSplitter* splitter_ = nullptr;
  std::function<void(const QString&)> open_path_;
};

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/collection/file_collection.hpp"
#include "dirtoo/fs/location.hpp"
#include "dirtoo/watcher/directory_watcher.hpp"

#include <QMainWindow>
#include <QStringListModel>

class QLineEdit;
class QListView;
class QLabel;

namespace dirtoo::app {

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);

  void open_location(const fs::Location& location);

private slots:
  void on_location_entered();
  void on_go_parent();
  void on_go_home();
  void on_directory_changed();
  void on_filter_changed(const QString& text);
  void on_item_activated(const QModelIndex& index);

private:
  void refresh_list();

  fs::Location location_;
  collection::FileCollection collection_;
  watcher::DirectoryWatcher watcher_;

  QLineEdit* location_edit_ = nullptr;
  QLineEdit* filter_edit_ = nullptr;
  QListView* list_view_ = nullptr;
  QStringListModel* list_model_ = nullptr;
  QLabel* status_label_ = nullptr;
};

} // namespace dirtoo::app

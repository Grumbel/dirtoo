// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/collection/file_collection.hpp"
#include "dirtoo/fs/location.hpp"
#include "dirtoo/watcher/directory_watcher.hpp"

#include <QMainWindow>
#include <QStringListModel>

#include <vector>

class QLineEdit;
class QListView;
class QLabel;
class QAction;

namespace dirtoo::app {

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);

  void open_location(const fs::Location& location, bool record_history = true);

private slots:
  void on_location_entered();
  void on_go_parent();
  void on_go_home();
  void on_go_back();
  void on_go_forward();
  void on_directory_changed();
  void on_filter_changed(const QString& text);
  void on_item_activated(const QModelIndex& index);
  void on_context_menu(const QPoint& pos);
  void on_mkdir();
  void on_rename_selected();
  void on_delete_selected();

private:
  void refresh_list();
  void update_history_actions();
  [[nodiscard]] std::vector<fs::FileInfo> selected_fileinfos() const;

  fs::Location location_;
  collection::FileCollection collection_;
  watcher::DirectoryWatcher watcher_;

  std::vector<fs::Location> history_;
  int history_index_ = -1;

  QLineEdit* location_edit_ = nullptr;
  QLineEdit* filter_edit_ = nullptr;
  QListView* list_view_ = nullptr;
  QStringListModel* list_model_ = nullptr;
  QLabel* status_label_ = nullptr;

  QAction* back_act_ = nullptr;
  QAction* forward_act_ = nullptr;
};

} // namespace dirtoo::app

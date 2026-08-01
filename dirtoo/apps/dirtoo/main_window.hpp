// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "clipboard.hpp"
#include "dirtoo/collection/file_collection.hpp"
#include "dirtoo/fs/location.hpp"
#include "dirtoo/thumbnail/thumbnailer.hpp"
#include "dirtoo/watcher/directory_watcher.hpp"
#include "dirops/ops.hpp"
#include "file_list_model.hpp"

#include <QMainWindow>

#include <filesystem>
#include <vector>

class QLineEdit;
class QTreeView;
class QListView;
class QLabel;
class QAction;
class QStackedWidget;

namespace dirtoo::app {

enum class ViewMode {
  Detail,
  Icons,
};

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
  void on_copy();
  void on_cut();
  void on_paste();
  void on_header_clicked(int section);
  void on_view_detail();
  void on_view_icons();
  void on_thumbnail_ready(const dirtoo::fs::Location& location, const QString& path);
  void on_thumbnail_failed(const dirtoo::fs::Location& location, const QString& message);

private:
  void refresh_list();
  void update_history_actions();
  void update_edit_actions();
  void set_view_mode(ViewMode mode);
  void set_clipboard(ClipboardMode mode);
  void request_thumbnails_for_visible();
  [[nodiscard]] std::vector<fs::FileInfo> selected_fileinfos() const;
  [[nodiscard]] QAbstractItemView* current_view() const;

  fs::Location location_;
  collection::FileCollection collection_;
  watcher::DirectoryWatcher watcher_;
  thumbnail::Thumbnailer thumbnailer_;
  FileListModel* model_ = nullptr;

  std::vector<fs::Location> history_;
  int history_index_ = -1;

  enum class SortColumn { Name, Size, Modified, Type };
  SortColumn sort_column_ = SortColumn::Name;
  bool sort_ascending_ = true;
  ViewMode view_mode_ = ViewMode::Detail;

  QLineEdit* location_edit_ = nullptr;
  QLineEdit* filter_edit_ = nullptr;
  QStackedWidget* view_stack_ = nullptr;
  QTreeView* tree_view_ = nullptr;
  QListView* icon_view_ = nullptr;
  QLabel* status_label_ = nullptr;

  QAction* back_act_ = nullptr;
  QAction* forward_act_ = nullptr;
  QAction* paste_act_ = nullptr;
  QAction* detail_act_ = nullptr;
  QAction* icons_act_ = nullptr;
};

} // namespace dirtoo::app

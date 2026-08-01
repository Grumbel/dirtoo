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
#include "transfer_dialog.hpp"
#include "transfer_worker.hpp"

#include <QMainWindow>
#include <QThread>

#include <filesystem>
#include <vector>

class QLineEdit;
class QTreeView;
class QListView;
class QLabel;
class QAction;
class QStackedWidget;
class QAbstractItemView;
class QCloseEvent;

namespace dirtoo::app {

enum class ViewMode {
  Detail,
  Icons,
};

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  void open_location(const fs::Location& location, bool record_history = true);

protected:
  void closeEvent(QCloseEvent* event) override;

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
  void on_properties();
  void on_copy();
  void on_cut();
  void on_paste();
  void on_header_clicked(int section);
  void on_view_detail();
  void on_view_icons();
  void on_zoom_in();
  void on_zoom_out();
  void on_thumbnail_ready(const dirtoo::fs::Location& location, const QString& path);
  void on_thumbnail_failed(const dirtoo::fs::Location& location, const QString& message);
  void on_selection_changed();
  void on_urls_dropped(const QList<QUrl>& urls, Qt::DropAction action);

  void on_transfer_item_started(int index, int total, const QString& path);
  void on_transfer_byte_progress(quint64 done, quint64 total, const QString& path);
  void on_transfer_conflict(const QString& destination_name);
  void on_transfer_finished(TransferSummary summary);

private:
  void refresh_list();
  void update_history_actions();
  void update_edit_actions();
  void update_status_selection();
  void apply_icon_zoom();
  void set_view_mode(ViewMode mode);
  void set_clipboard(ClipboardMode mode);
  void request_thumbnails_for_visible();
  void start_transfer(const TransferRequest& request);
  void restore_settings();
  void persist_settings() const;
  [[nodiscard]] std::vector<fs::FileInfo> selected_fileinfos() const;
  [[nodiscard]] QAbstractItemView* current_view() const;

  fs::Location location_;
  collection::FileCollection collection_;
  watcher::DirectoryWatcher watcher_;
  thumbnail::Thumbnailer thumbnailer_;
  FileListModel* model_ = nullptr;

  QThread transfer_thread_;
  TransferWorker* transfer_worker_ = nullptr;
  TransferDialog* transfer_dialog_ = nullptr;
  bool transfer_busy_ = false;
  ClipboardMode last_transfer_mode_ = ClipboardMode::Copy;

  std::vector<fs::Location> history_;
  int history_index_ = -1;

  enum class SortColumn { Name, Size, Modified, Type };
  SortColumn sort_column_ = SortColumn::Name;
  bool sort_ascending_ = true;
  ViewMode view_mode_ = ViewMode::Detail;

  static constexpr int kZoomLevels[] = {48, 64, 96, 128, 192};
  int zoom_index_ = 2;

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

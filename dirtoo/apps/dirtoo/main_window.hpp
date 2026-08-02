// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "clipboard.hpp"
#include "dirtoo/archive/archive_index.hpp"
#include "dirtoo/archive/archive_manager.hpp"
#include "dirtoo/collection/file_collection.hpp"
#include "dirtoo/fs/location.hpp"
#include "dirtoo/thumbnail/thumbnailer.hpp"
#include "dirtoo/watcher/directory_watcher.hpp"
#include "dirops/ops.hpp"
#include "app_settings.hpp"
#include "bookmarks.hpp"
#include "file_list_model.hpp"
#include "history_menu.hpp"
#include "message_area.hpp"
#include "search_worker.hpp"
#include "path_completion_worker.hpp"
#include "directory_load_worker.hpp"
#include "sort_worker.hpp"
#include "filter_worker.hpp"
#include "directory_thumbnail_worker.hpp"
#include "leap_widget.hpp"
#include "location_button_bar.hpp"
#include "transfer_dialog.hpp"
#include "transfer_worker.hpp"
#include "graphics_file_view.hpp"

#include <QMainWindow>
class QListWidgetItem;
#include <QSet>
#include <QThread>

#include <filesystem>
#include <vector>

class QLineEdit;
class QTreeView;
class QSplitter;
class QListWidget;
class QLabel;
class QListView;
class GraphicsFileView;
class QLabel;
class QAction;
class QStackedWidget;
class QAbstractItemView;
class QCloseEvent;
class QStringListModel;
class QCompleter;
class QTimer;

namespace dirtoo::app {

enum class ViewMode {
  Detail,
  Icons,
  SmallIcons, // compact list-like icon rows (Python SequenceMode / small icon view)
};

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  void open_location(const fs::Location& location, bool record_history = true);

  /// Open an independent MainWindow (Qt::WA_DeleteOnClose).
  static MainWindow* open_new_window(const fs::Location& location);

protected:
  void closeEvent(QCloseEvent* event) override;
  void showEvent(QShowEvent* event) override;

private slots:
  void on_location_entered();
  void on_go_parent();
  void on_go_home();
  void on_go_back();
  void on_go_forward();
  void on_directory_changed();
  /// @param soft If true (watcher), keep current listing until load completes.
  void reload_directory(bool soft);
  void on_directory_loaded(quint64 generation, std::vector<dirtoo::fs::FileInfo> items);
  void on_directory_load_failed(quint64 generation, QString error);
  void on_sort_finished(quint64 generation, std::vector<dirtoo::fs::FileInfo> items);
  void request_async_sort();
  void update_detail_row_heights();
  void on_show_filter_help();
  void on_filter_changed(const QString& text);
  void on_filter_finished(quint64 generation, std::vector<dirtoo::fs::FileInfo> visible, bool parse_ok);
  /// @param keep_previous_visible Soft reload: do not clear the list until filter finishes.
  void request_async_filter(bool keep_previous_visible = false);
  void on_paste_link();
  void on_item_activated(const QModelIndex& index);
  void on_context_menu(const QPoint& pos);
  void on_mkdir();
  void on_create_file();
  void on_swap_names();
  void on_toggle_show_abspath(bool checked);
  void on_rename_selected();
  void on_delete_selected();
  void on_properties();
  void on_refresh();
  void on_open_with();
  void on_open_terminal();
  void on_toggle_hidden(bool checked);
  void on_toggle_sidebar(bool checked);
  void on_sidebar_activated(const QModelIndex& index);
  void sync_sidebar_to_location();
  void rebuild_sidebar_places();
  void on_udisks_volumes_changed();
  void on_devices_item_activated(QListWidgetItem* item);
  void on_about();
  void on_preferences();
  void on_reload_thumbnails();
  void on_prepare_thumbnails();
  void on_make_directory_thumbnails();
  void apply_settings(const AppSettings& settings);
  void on_archive_ready(const dirtoo::fs::Location& archive_location,
                        const std::filesystem::path& extracted_root);
  void on_archive_failed(const dirtoo::fs::Location& archive_location, const QString& message);
  void on_focus_location();
  void on_breadcrumb_location(const dirtoo::fs::Location& location);
  void on_breadcrumb_location_new_window(const dirtoo::fs::Location& location);
  void on_new_window();
  void on_breadcrumb_drop(const dirtoo::fs::Location& target, const QList<QUrl>& urls,
                         Qt::DropAction action);
  void on_clear_filter();
  void on_view_middle_click(const QModelIndex& index);
  void on_leap(const QString& text, bool forward, bool from_key);
  void on_show_leap();
  /// Select and scroll to a model row (Home/End / leap).
  void jump_to_row(int row);
  void on_toggle_filter_visible();
  void on_show_search();
  void on_search_submitted();
  void on_search_match(const QString& path, bool is_directory, quint64 size);
  void on_search_finished(quint64 matched, quint64 visited, const QString& error);
  void on_search_progress(quint64 visited, quint64 matched);
  void stop_search();
  void on_location_text_edited(const QString& text);
  void on_path_completion_timeout();
  void on_path_completions_ready(quint64 request_id, const QString& longest,
                                const QStringList& candidates);
  void on_rebuild_history_menu();
  void on_rebuild_bookmarks_menu();
  void on_toggle_bookmark();
  void on_parent_new_window();
  bool eventFilter(QObject* obj, QEvent* event) override;
  void on_location_edit_requested();
  void show_location_buttons();
  void show_location_line_edit();
  void on_copy();
  void on_cut();
  void on_paste();
  void on_header_clicked(int section);
  void on_view_detail();
  void on_view_icons();
  void on_view_small_icons();
  void on_zoom_in();
  void on_zoom_out();
  void on_more_icon_details();
  void on_less_icon_details();
  void apply_icon_detail_level();
  void on_thumbnail_ready(const dirtoo::fs::Location& location, const QString& path);
  void on_thumbnail_failed(const dirtoo::fs::Location& location, const QString& message);
  void on_selection_changed();
  void on_urls_dropped(const QList<QUrl>& urls, Qt::DropAction action);
  void on_urls_dropped_to(const QList<QUrl>& urls, Qt::DropAction action, const QString& dest_dir);
  void on_save_file_list();
  void on_select_all();

  void on_transfer_item_started(int index, int total, const QString& path);
  void on_transfer_byte_progress(quint64 done, quint64 total, const QString& path);
  void on_transfer_conflict(const QString& destination_name, const QString& source_path,
                            const QString& destination_path);
  void on_transfer_finished(TransferSummary summary);

private:
  void refresh_list();
  void update_history_actions();
  void update_edit_actions();
  void update_status_selection();
  void update_filter_chrome(bool filtered);
  void apply_icon_zoom();
  void set_view_mode(ViewMode mode);
  /// Update status bar and mirror the text to qInfo (visible with --verbose).
  void set_status(const QString& text);
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
  archive::ArchiveManager archive_manager_;
  fs::Location pending_archive_location_;
  std::vector<archive::ArchiveEntry> archive_entries_;
  bool archive_listing_ok_ = false;
  std::filesystem::path indexed_archive_path_;
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

  static constexpr int kZoomLevels[] = {48, 64, 96, 128, 192, 256, 384, 512, 768, 1024};
  int zoom_index_ = 2;

  QLineEdit* location_edit_ = nullptr;
  LocationButtonBar* location_buttons_ = nullptr;
  LeapWidget* leap_widget_ = nullptr;
  QAction* parent_act_ = nullptr;
  QAction* show_filter_act_ = nullptr;
  QAction* pin_filter_act_ = nullptr;
  bool filter_pinned_ = false;
  bool show_abspath_ = false;
  HistoryMenu* history_menu_ = nullptr;
  HistoryMenu* bookmarks_menu_ = nullptr;
  Bookmarks bookmarks_{Bookmarks::default_path()};
  std::vector<fs::Location> location_history_unique_;
  QWidget* location_stack_host_ = nullptr;
  QSplitter* main_splitter_ = nullptr;
  QWidget* sidebar_widget_ = nullptr;
  QTreeView* sidebar_tree_ = nullptr;
  class DirectoryTreeModel* directory_tree_model_ = nullptr;
  class UDisksClient* udisks_client_ = nullptr;
  class QListWidget* devices_list_ = nullptr;
  class QLabel* devices_label_ = nullptr;
  QAction* show_sidebar_act_ = nullptr;
  QWidget* filter_row_ = nullptr;
  QLineEdit* filter_edit_ = nullptr;
  QStringList filter_history_;
  int filter_history_index_ = -1;
  QWidget* search_row_ = nullptr;
  QLineEdit* search_edit_ = nullptr;
  bool search_active_ = false;
  std::vector<fs::FileInfo> search_results_;
  QThread* search_thread_ = nullptr;
  SearchWorker* search_worker_ = nullptr;
  QThread* path_completion_thread_ = nullptr;
  PathCompletionWorker* path_completion_worker_ = nullptr;
  QThread* dir_load_thread_ = nullptr;
  DirectoryLoadWorker* dir_load_worker_ = nullptr;
  quint64 dir_load_generation_ = 0;
  bool soft_directory_reload_ = false;
  QSet<QString> known_paths_;
  fs::Location known_paths_location_;
  QThread* sort_thread_ = nullptr;
  SortWorker* sort_worker_ = nullptr;
  quint64 sort_generation_ = 0;
  QThread* filter_thread_ = nullptr;
  FilterWorker* filter_worker_ = nullptr;
  quint64 filter_generation_ = 0;
  QThread* dir_thumb_thread_ = nullptr;
  DirectoryThumbnailWorker* dir_thumb_worker_ = nullptr;
  QStringListModel* path_completion_model_ = nullptr;
  QCompleter* path_completer_ = nullptr;
  QTimer* path_completion_timer_ = nullptr;
  QTimer* watcher_reload_timer_ = nullptr;
  QString path_completion_pending_;
  quint64 path_completion_request_id_ = 0;
  QStackedWidget* view_stack_ = nullptr;
  QTreeView* tree_view_ = nullptr;
  QListView* icon_view_ = nullptr; // SmallIcons list mode
  GraphicsFileView* graphics_view_ = nullptr; // Icons Graphics View
  QLabel* status_label_ = nullptr;
  MessageArea* message_area_ = nullptr;

  QAction* back_act_ = nullptr;
  QAction* forward_act_ = nullptr;
  QAction* paste_act_ = nullptr;
  QAction* detail_act_ = nullptr;
  QAction* icons_act_ = nullptr;
  QAction* small_icons_act_ = nullptr;
  QAction* crop_thumbnails_act_ = nullptr;
  QAction* show_hidden_act_ = nullptr;
};

} // namespace dirtoo::app

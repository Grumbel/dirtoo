// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "archive_listing.hpp"
#include "clipboard.hpp"
#include "dirtoo/archive/archive_index.hpp"
#include "dirtoo/archive/archive_manager.hpp"
#include "dirtoo/collection/file_collection.hpp"
#include "dirtoo/fs/location.hpp"
#include "dirtoo/watcher/directory_watcher.hpp"
#include "dirops/ops.hpp"
#include "app_settings.hpp"
#include "bookmarks.hpp"
#include "file_list_model.hpp"
#include "navigation_history.hpp"
#include "search_controller.hpp"
#include "transfer_controller.hpp"
#include "devices_controller.hpp"
#include "history_menu.hpp"
#include "message_area.hpp"
#include "search_worker.hpp"
#include "location_chrome.hpp"
#include "list_pipeline_workers.hpp"
#include "sidebar_controller.hpp"
#include "view_mode.hpp"
#include "view_zoom.hpp"
#include "filter_history.hpp"
#include "directory_session.hpp"
#include "search_session.hpp"
#include "directory_load_worker.hpp"
#include "sort_worker.hpp"
#include "filter_worker.hpp"
#include "thumbnail_coordinator.hpp"
#include "leap_widget.hpp"
#include "transfer_dialog.hpp"
#include "transfer_worker.hpp"
#include "graphics_file_view.hpp"

#include <QMainWindow>
class QListWidgetItem;
#include <QHash>
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
class QToolButton;

namespace dirtoo::app {


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
  void on_location_entered(const QString& text);
  void on_go_parent();
  void on_go_home();
  void on_go_back();
  void on_go_forward();
  /// Right-click on toolbar Back/Forward: list stack entries in that direction.
  void on_back_history_menu(const QPoint& pos);
  void on_forward_history_menu(const QPoint& pos);
  void on_directory_changed();
  void on_entries_changed(const QStringList& created, const QStringList& removed,
                           const QStringList& modified);
  /// Apply FileInfo results produced off-thread for watcher create/modify deltas.
  void apply_watcher_upserts(std::vector<dirtoo::fs::FileInfo> infos,
                             const QStringList& created_paths);
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
  void request_async_filter(bool keep_previous_visible = true);
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
  void on_toggle_read_only(bool checked);
  void on_sidebar_activated(const QModelIndex& index);
  void sync_sidebar_to_location();
  void rebuild_sidebar_places();
  void on_about();
  void on_preferences();
  void on_checksums();
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
  /// Always show the filter bar and focus the filter line edit (Ctrl+K).
  void on_focus_filter();
  void on_show_search();
  void on_search_submitted();
  void on_search_match(const QString& path, bool is_directory, quint64 size);
  void flush_search_batch();
  void on_search_finished(quint64 matched, quint64 visited, const QString& error);
  void on_search_progress(quint64 visited, quint64 matched);
  void stop_search();
  void on_rebuild_history_menu();
  void on_rebuild_recent_opens_menu();
  void on_rebuild_bookmarks_menu();
  void on_toggle_bookmark();
  void on_parent_new_window();
  bool eventFilter(QObject* obj, QEvent* event) override;
  void show_location_buttons();
  void on_copy();
  void on_cut();
  void on_paste();
  void on_header_clicked(int section);
  void on_view_detail();
  void on_view_icons();
  void on_view_relative_icons();
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
  /// Show/hide the status-bar busy indicator and set its tooltip.
  void update_busy_indicator(const QString& activity = {});
  void set_clipboard(ClipboardMode mode);
  void request_thumbnails_for_visible();
  void cancel_all_thumbnails();
  void clear_thumb_aliases();
  void request_thumbnails_for_paths(const std::vector<dirtoo::fs::Location>& locs,
                                  const QStringList& mimes);
  void wire_thumbnail_services();
  void shutdown_thumbnail_workers();
  /// Queue montage builds for visible directories after a delay (low priority).
  void schedule_directory_thumbnails_low_priority();
  void start_transfer(const TransferRequest& request);
  /// Resolve drop/paste URLs to real paths (extract archive members off-thread if needed).
  void begin_transfer_from_urls(const QList<QUrl>& urls, Qt::DropAction action,
                               const std::filesystem::path& dest_dir);
  /// Watch real dir, archive extract tree, or archive file as appropriate.
  void start_watcher_for_location();
  void setup_background_workers();
  void setup_toolbar();
  void setup_menus();
  void setup_file_menu();
  void setup_edit_menu();
  void setup_view_menu();
  void setup_sort_menu();
  void setup_go_help_menus();
  void setup_shortcuts();
  void setup_central_ui();
  void setup_status_and_services();
  void restore_settings();
  void persist_settings() const;
  [[nodiscard]] std::vector<fs::FileInfo> selected_fileinfos() const;
  [[nodiscard]] QAbstractItemView* current_view() const;

  fs::Location location_;
  collection::FileCollection collection_;

  ThumbnailCoordinator thumbs_{this};
  LocationChrome location_chrome_{this};
  ListPipelineWorkers list_workers_{this};
  watcher::DirectoryWatcher watcher_;
  archive::ArchiveManager archive_manager_;
  ArchiveListing archive_listing_;
  DirectorySession dir_session_;
  FileListModel* model_ = nullptr;

  TransferController transfer_controller_;
  NavigationHistory nav_history_;

  /// Tracks current sort for toolbar label / header toggle (mirrors collection SortKey).
  collection::SortKey sort_key_ = collection::SortKey::Name;
  bool sort_ascending_ = true;
  void apply_sort_key(collection::SortKey key, bool toggle_if_same);
  void update_sort_toolbar_label();
  ViewMode view_mode_ = ViewMode::Detail;

  ViewZoom zoom_;
  QStringList detail_columns_;
  void apply_detail_column_visibility();
  [[nodiscard]] int& zoom_for_current_view() { return zoom_.for_mode(view_mode_); }
  [[nodiscard]] int zoom_for_current_view() const { return zoom_.for_mode(view_mode_); }

  LeapWidget* leap_widget_ = nullptr;
  QAction* parent_act_ = nullptr;
  QAction* show_filter_act_ = nullptr;
  QAction* pin_filter_act_ = nullptr;
  bool filter_pinned_ = false;
  bool show_abspath_ = false;
  HistoryMenu* history_menu_ = nullptr;
  HistoryMenu* bookmarks_menu_ = nullptr;
  QMenu* recent_opens_menu_ = nullptr;
  Bookmarks bookmarks_{Bookmarks::default_path()};
  QWidget* location_stack_host_ = nullptr;
  QSplitter* main_splitter_ = nullptr;
  QWidget* sidebar_widget_ = nullptr;
  QTreeView* sidebar_tree_ = nullptr;
  SidebarController sidebar_{this};
  DevicesController* devices_controller_ = nullptr;
  class QListWidget* devices_list_ = nullptr;
  class QLabel* devices_label_ = nullptr;
  QAction* show_sidebar_act_ = nullptr;
  QAction* read_only_act_ = nullptr;
  bool read_only_ = false;
  /// Returns false (and sets status) when mutations are blocked.
  [[nodiscard]] bool ensure_mutations_allowed();
  void update_mutation_actions();
  /// Window title: shortened path first, then "dirtoo" [read-only].
  void update_window_title();
  QWidget* filter_row_ = nullptr;
  QLineEdit* filter_edit_ = nullptr;
  FilterHistory filter_history_;
  QWidget* search_row_ = nullptr;
  QLineEdit* search_edit_ = nullptr;
  SearchSession search_session_;
  SearchController search_controller_;
  QTimer* watcher_reload_timer_ = nullptr;
  QStackedWidget* view_stack_ = nullptr;
  QTreeView* tree_view_ = nullptr;
  QListView* icon_view_ = nullptr; // List view (and fallback if no Graphics)
  GraphicsFileView* graphics_view_ = nullptr; // Icons Graphics View
  /// Left side of the status bar: current filename / transient messages.
  QLabel* status_label_ = nullptr;
  /// Busy indicator (loading badge) shown while background IO / workers run.
  QLabel* busy_label_ = nullptr;
  /// Right (permanent) side: directory visible/total size and selection size.
  QLabel* status_info_label_ = nullptr;
  MessageArea* message_area_ = nullptr;

  QAction* back_act_ = nullptr;
  QAction* forward_act_ = nullptr;
  QAction* paste_act_ = nullptr;
  QAction* detail_act_ = nullptr;
  QAction* icons_act_ = nullptr;
  QAction* relative_icons_act_ = nullptr;
  QAction* small_icons_act_ = nullptr;
  QToolButton* sort_toolbar_btn_ = nullptr;
  QToolButton* group_toolbar_btn_ = nullptr;
  QAction* crop_thumbnails_act_ = nullptr;
  QAction* show_hidden_act_ = nullptr;
};

} // namespace dirtoo::app

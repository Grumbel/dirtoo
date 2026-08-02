// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"
#include "badge_icons.hpp"
#include "theme_icons.hpp"
#include "location_icons.hpp"
#include "location_url.hpp"
#include "location_menu_helpers.hpp"
#include "directory_tree_model.hpp"
#include "udisks_client.hpp"
#include "devices_controller.hpp"
#include "filter_worker.hpp"
#include "directory_thumbnail_worker.hpp"
#include "file_views.hpp"
#include "file_context_menu.hpp"
#include "file_list_model.hpp"
#include "graphics_file_view.hpp"
#include "graphics_file_item.hpp"
#include "file_item_delegate.hpp"
#include "dirtoo/filter/media_meta_cache.hpp"

#include "dirtoo/filter/parser.hpp"

#include "clipboard.hpp"
#include "about_dialog.hpp"
#include "name_input_dialog.hpp"
#include "app_settings.hpp"
#include "size_format.hpp"
#include "conflict_dialog.hpp"
#include "open_with.hpp"
#include "open_history.hpp"
#include "operations_history.hpp"
#include "preferences_dialog.hpp"
#include "properties_dialog.hpp"
#include "archive_member_cache.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/thumbnail/thumbnailer.hpp"
#include "dirops/ops.hpp"
#include "dirops/util.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QtConcurrent>
#include "dirtoo/archive/archive_index.hpp"
#include <QApplication>
#include <QDateTime>
#include <QCoreApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QShowEvent>
#include <QCompleter>
#include <QStringListModel>
#include <QTimer>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QFrame>
#include <QFileDialog>
#include <QTextStream>
#include <QMouseEvent>
#include <QDesktopServices>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <set>
#include <QHeaderView>
#include <QHash>
#include <QIcon>
#include <QPainter>
#include <QSvgRenderer>
#include <QKeyEvent>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMimeDatabase>
#include <QPixmap>
#include <QProcess>
#include <QSplitter>
#include <QStyle>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QScrollBar>
#include <QUrl>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QBrush>
#include <QColor>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <algorithm>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <filesystem>
#include <optional>

namespace dirtoo::app {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
  setWindowTitle(QStringLiteral("dirtoo"));
  resize(960, 640);

  setup_background_workers();
  setup_toolbar();
  setup_menus();
  setup_shortcuts();
  setup_central_ui();
  setup_status_and_services();

  update_history_actions();
  update_edit_actions();
  set_view_mode(ViewMode::Detail);
  restore_settings();
}

MainWindow::~MainWindow()
{
  stop_search();
  if (path_completion_worker_ != nullptr) {
    path_completion_worker_->cancel();
  }
  if (path_completion_thread_ != nullptr) {
    path_completion_thread_->quit();
    path_completion_thread_->wait(2000);
  }
  transfer_controller_.shutdown();
  search_controller_.stop();
  if (dir_load_thread_ != nullptr) {
    dir_load_thread_->quit();
    dir_load_thread_->wait(3000);
  }
  if (sort_thread_ != nullptr) {
    sort_thread_->quit();
    sort_thread_->wait(3000);
  }
  if (filter_thread_ != nullptr) {
    filter_thread_->quit();
    filter_thread_->wait(3000);
  }
  if (dir_thumb_thread_ != nullptr) {
    dir_thumb_thread_->quit();
    dir_thumb_thread_->wait(3000);
  }
}

QAbstractItemView* MainWindow::current_view() const
{
  if (view_mode_ == ViewMode::Detail) {
    return tree_view_;
  }
  return icon_view_;
}


void MainWindow::apply_icon_zoom()
{
  if (view_mode_ == ViewMode::SmallIcons) {
    // Windows 95 Explorer "List" view: small icon left of filename, columns
    // filled top-to-bottom then left-to-right.
    static constexpr int kSmall[] = {16, 24, 32, 48, 64, 96, 128};
    const int zi = std::clamp(zoom_list_, 0, static_cast<int>(std::size(kSmall)) - 1);
    const int size = kSmall[zi];
    icon_view_->setViewMode(QListView::ListMode);
    icon_view_->setFlow(QListView::TopToBottom);
    icon_view_->setWrapping(true);
    icon_view_->setResizeMode(QListView::Adjust);
    icon_view_->setMovement(QListView::Static);
    icon_view_->setUniformItemSizes(true);
    icon_view_->setWordWrap(false);
    icon_view_->setIconSize(QSize(size, size));
    icon_view_->setSpacing(2);
    // Row a little taller than the icon/filename; column wide enough for a name.
    const int row_h = std::max(size, 16) + 6;
    const int col_w = std::max(size + 12 + 140, 180);
    icon_view_->setGridSize(QSize(col_w, row_h));
    if (model_ != nullptr) {
      // List layout: Decoration left, DisplayRole text to the right (not under icon).
      model_->set_icon_style(false);
      model_->set_icon_detail_level(1);
    }
    return;
  }

  if (view_mode_ == ViewMode::Detail) {
    static constexpr int kDetail[] = {16, 24, 32, 48, 64, 96, 128};
    const int zi = std::clamp(zoom_detail_, 0, static_cast<int>(std::size(kDetail)) - 1);
    const int size = kDetail[zi];
    if (tree_view_ != nullptr) {
      tree_view_->setIconSize(QSize(size, size));
    }
    return;
  }

  const int size = kZoomLevels[std::clamp(zoom_icons_, 0, static_cast<int>(std::size(kZoomLevels)) - 1)];
  if (view_mode_ == ViewMode::Icons) {
    icon_view_->setViewMode(QListView::IconMode);
    icon_view_->setFlow(QListView::LeftToRight);
    icon_view_->setWrapping(true);
    icon_view_->setResizeMode(QListView::Adjust);
    icon_view_->setUniformItemSizes(true);
  }
  icon_view_->setIconSize(QSize(size, size));
  const int text_rows = model_ != nullptr ? model_->icon_text_rows() : 1;
  const int text_h = 6 + text_rows * 18;
  const int cell_w = std::max(size + 40, 96);
  const int cell_h = size + text_h + 16;
  icon_view_->setGridSize(QSize(cell_w, cell_h));
  icon_view_->setSpacing(8);
  if (graphics_view_ != nullptr) {
    graphics_view_->set_tile_size(QSize(cell_w, cell_h));
    graphics_view_->set_compact(false);
    graphics_view_->relayout();
  }
  const int detail = std::max(16, size / 4);
  tree_view_->setIconSize(QSize(detail, detail));
}

void MainWindow::apply_icon_detail_level()
{
  if (model_ == nullptr) {
    return;
  }
  if (status_label_ != nullptr && view_mode_ == ViewMode::Icons) {
    static const char* labels[] = {
        "Icon captions: none",
        "Icon captions: name",
        "Icon captions: name",
        "Icon captions: name + size",
        "Icon captions: name + size + date",
    };
    const int lvl = model_->icon_detail_level();
    set_status(QString::fromUtf8(labels[std::clamp(lvl, 0, 4)]));
  }
}

void MainWindow::on_more_icon_details()
{
  if (model_ == nullptr) {
    return;
  }
  model_->set_icon_detail_level(model_->icon_detail_level() + 1);
  apply_icon_zoom();
  apply_icon_detail_level();
}

void MainWindow::on_less_icon_details()
{
  if (model_ == nullptr) {
    return;
  }
  model_->set_icon_detail_level(model_->icon_detail_level() - 1);
  apply_icon_zoom();
  apply_icon_detail_level();
}

int& MainWindow::zoom_for_current_view()
{
  switch (view_mode_) {
  case ViewMode::SmallIcons:
    return zoom_list_;
  case ViewMode::Detail:
    return zoom_detail_;
  case ViewMode::Icons:
  default:
    return zoom_icons_;
  }
}

int MainWindow::zoom_for_current_view() const
{
  switch (view_mode_) {
  case ViewMode::SmallIcons:
    return zoom_list_;
  case ViewMode::Detail:
    return zoom_detail_;
  case ViewMode::Icons:
  default:
    return zoom_icons_;
  }
}

void MainWindow::on_zoom_in()
{
  const int max_zi = (view_mode_ == ViewMode::Icons)
                         ? static_cast<int>(std::size(kZoomLevels)) - 1
                         : 6;
  int& zi = zoom_for_current_view();
  if (zi < max_zi) {
    ++zi;
    apply_icon_zoom();
  }
}

void MainWindow::on_zoom_out()
{
  int& zi = zoom_for_current_view();
  if (zi > 0) {
    --zi;
    apply_icon_zoom();
  }
}


void MainWindow::set_view_mode(ViewMode mode)
{
  view_mode_ = mode;
  if (mode == ViewMode::Detail) {
    if (model_ != nullptr) {
      model_->set_icon_style(false);
    }
    view_stack_->setCurrentWidget(tree_view_);
    if (detail_act_ != nullptr) {
      detail_act_->setChecked(true);
    }
    apply_icon_zoom();
    apply_detail_column_visibility();
    request_thumbnails_for_visible();
  } else if (mode == ViewMode::SmallIcons) {
    if (model_ != nullptr) {
      model_->set_icon_style(false);
      model_->set_icon_detail_level(1);
    }
    view_stack_->setCurrentWidget(icon_view_);
    if (small_icons_act_ != nullptr) {
      small_icons_act_->setChecked(true);
    }
    apply_icon_zoom();
    request_thumbnails_for_visible();
  } else {
    if (model_ != nullptr) {
      model_->set_icon_style(true);
      // List view forces detail level 1; restore a useful multi-line caption LOD for Icons.
      if (model_->icon_detail_level() <= 1) {
        model_->set_icon_detail_level(3);
      }
    }
    if (graphics_view_ != nullptr) {
      view_stack_->setCurrentWidget(graphics_view_);
      graphics_view_->sync_from_model();
    } else {
      view_stack_->setCurrentWidget(icon_view_);
    }
    if (icons_act_ != nullptr) {
      icons_act_->setChecked(true);
    }
    apply_icon_zoom();
    request_thumbnails_for_visible();
  }
}

void MainWindow::on_view_detail()
{
  set_view_mode(ViewMode::Detail);
}

void MainWindow::on_view_icons()
{
  set_view_mode(ViewMode::Icons);
}

void MainWindow::on_view_small_icons()
{
  set_view_mode(ViewMode::SmallIcons);
}

void MainWindow::set_status(const QString& text)
{
  if (status_label_ != nullptr) {
    status_label_->setText(text);
  }
  if (!text.isEmpty()) {
    qInfo().noquote() << QStringLiteral("status: %1").arg(text);
  }
}

void MainWindow::open_location(const fs::Location& location, bool record_history)
{

  qInfo().noquote() << QStringLiteral("open_location %1 (history=%2)")
                           .arg(QString::fromStdString(location.as_url()))
                           .arg(record_history);
  stop_search();
  search_active_ = false;
  search_results_.clear();
  if (search_row_ != nullptr ? search_row_->isVisible()
      : (search_edit_ != nullptr && search_edit_->isVisible())) {
    if (search_row_ != nullptr) {
      search_row_->hide();
    } else if (search_edit_ != nullptr) {
      search_edit_->hide();
    }
  }
  // Reset filter on directory change unless Pin Filter is active.
  if (!filter_pinned_ && filter_edit_ != nullptr && !filter_edit_->text().isEmpty()) {
    filter_edit_->blockSignals(true);
    filter_edit_->clear();
    filter_edit_->blockSignals(false);
    collection_.set_name_filter(std::string{});
    update_filter_chrome(false);
  }
  location_ = location;
  if (location_.is_archive()) {
    location_edit_->setText(QString::fromStdString(location_.as_url()));
  } else {
    location_edit_->setText(QString::fromStdString(location_.as_path().string()));
  }
  if (location_buttons_ != nullptr) {
    location_buttons_->set_location(location_);
  }
  show_location_buttons();

  nav_history_.push(location, record_history);
  update_history_actions();

  if (location_.is_archive()) {
    pending_archive_location_ = location_;

    // Reuse index when navigating within the same archive file.
    if (archive_listing_.ready_for(location_.as_path())) {
      start_watcher_for_location();
      on_directory_changed();
    } else {
      QApplication::setOverrideCursor(Qt::WaitCursor);
      std::string list_err;
      const bool listed = archive_listing_.load(location_.as_path(), &list_err);
      QApplication::restoreOverrideCursor();
      if (listed) {
        set_status(QStringLiteral("Archive index: %1 entries")
                                   .arg(archive_listing_.entries().size()));
        start_watcher_for_location();
        on_directory_changed();
      } else {
        set_status(QStringLiteral("Listing failed (%1); extracting…")
                                   .arg(QString::fromStdString(list_err)));
        if (archive_manager_.status(fs::Location::from_archive(location_.as_path(), {}))
            != archive::ExtractStatus::Ready) {
          QApplication::setOverrideCursor(Qt::WaitCursor);
        }
        archive_manager_.open(location_);
      }
    }
  } else {
    start_watcher_for_location();
    // Initial listing (watcher no longer emits on start).
    reload_directory(false);
  }

  if (auto* view = current_view()) {
    view->setFocus(Qt::OtherFocusReason);
  }
  sync_sidebar_to_location();

}

void MainWindow::on_location_entered()
{
  try {
    open_location(fs::Location::from_human(location_edit_->text().toStdString()));
  } catch (const std::exception& ex) {
    set_status(QString::fromUtf8(ex.what()));
  }
}

void MainWindow::on_go_parent()
{
  open_location(location_.parent());
}

void MainWindow::on_go_home()
{
  open_location(fs::Location::from_path(std::filesystem::path{QDir::homePath().toStdString()}));
}

void MainWindow::on_go_back()
{
  if (const auto loc = nav_history_.go_back()) {
    open_location(*loc, false);
  }
}

void MainWindow::on_go_forward()
{
  if (const auto loc = nav_history_.go_forward()) {
    open_location(*loc, false);
  }
}

void MainWindow::update_history_actions()
{
  if (back_act_) {
    back_act_->setEnabled(nav_history_.can_go_back());
  }
  if (forward_act_) {
    forward_act_->setEnabled(nav_history_.can_go_forward());
  }
}


void MainWindow::update_edit_actions()
{
  if (paste_act_) {
    paste_act_->setEnabled(!transfer_controller_.busy()
                           && clipboard_has_paths(QApplication::clipboard()->mimeData()));
  }
}

void MainWindow::on_directory_changed()
{
  reload_directory(false);
}

void MainWindow::reload_directory(bool soft)
{
  if (search_active_) {
    // Keep recursive search results until the user navigates away or closes search.
    return;
  }
  qInfo().noquote() << QStringLiteral("reload_directory soft=%1 path=%2")
                           .arg(soft)
                           .arg(QString::fromStdString(location_.as_url()));
  soft_directory_reload_ = soft;
  if (!soft) {
    // Navigation/explicit refresh supersedes any pending soft watcher tick.
    if (watcher_reload_timer_ != nullptr) {
      watcher_reload_timer_->stop();
    }
    thumbnailer_.cancel_all();
    thumb_alias_.clear();
    if (model_ != nullptr) {
      model_->clear_thumbnails();
    }
    filter::MediaMetaCache::instance().bump_generation();
  }

  // In-memory archive index: apply on UI thread (no directory walk).
  // Soft watcher ticks refresh the TOC only when the archive file stamp changes.
  if (location_.is_archive()) {
    std::string list_err;
    (void)archive_listing_.refresh_if_stale(location_.as_path(), &list_err);
  }
  if (location_.is_archive() && archive_listing_.ok()) {
    soft_directory_reload_ = false;
    auto items = archive_listing_.fileinfos_for(location_);
    if (model_ != nullptr) {
      model_->clear_child_counts();
      for (const auto& [path, n] : archive_listing_.child_counts_for(location_)) {
        model_->set_child_count(QString::fromStdString(path), static_cast<qint64>(n));
      }
    }
    collection_.sorter().set_ascending(sort_ascending_);
    collection_.set_items(std::move(items));
    if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()) {
      if (filter_expression_needs_content_io(filter_edit_->text())) {
        request_async_filter();
      } else {
        collection_.set_name_filter(filter_edit_->text().toStdString());
      }
    }
    refresh_list();
    request_thumbnails_for_visible();
    return;
  }

  fs::Location load_loc = location_;
  if (location_.is_archive()) {
    const auto resolved = archive_manager_.resolved_directory(location_);
    if (!resolved) {
      set_status(QStringLiteral("Archive not ready"));
      return;
    }
    load_loc = fs::Location::from_path(*resolved);
  }

  const quint64 gen = ++dir_load_generation_;
  set_status(soft ? QStringLiteral("Refreshing…") : QStringLiteral("Loading…"));
  // Hard navigation: clear immediately so the old directory does not linger.
  // Soft (watcher): keep showing the previous listing until the worker finishes.
  if (!soft) {
    collection_.clear();
    refresh_list();
  }

  if (dir_load_worker_ == nullptr) {
    return;
  }
  // Supersede any in-flight listing (especially important when soft refreshes stack up).
  QMetaObject::invokeMethod(dir_load_worker_, "cancel", Qt::QueuedConnection);
  const QString path = QString::fromStdString(load_loc.as_path().string());
  QMetaObject::invokeMethod(dir_load_worker_, "load", Qt::QueuedConnection,
                            Q_ARG(QString, path), Q_ARG(quint64, gen));
}

void MainWindow::on_directory_loaded(quint64 generation, std::vector<fs::FileInfo> items)
{
  qInfo().noquote() << QStringLiteral("directory_loaded gen=%1 items=%2")
                           .arg(generation)
                           .arg(items.size());

  if (generation != dir_load_generation_ || search_active_) {
    return;
  }
  // "New" badge: paths that appeared since we last listed this location.
  // Keep marks across soft watcher reloads (Python keeps _new until the item
  // is gone / the directory is left). Only clear when navigating away.
  if (model_ != nullptr) {
    model_->clear_child_counts();
    QSet<QString> next_paths;
    next_paths.reserve(static_cast<int>(items.size()));
    for (const auto& fi : items) {
      next_paths.insert(QString::fromStdString(fi.path().string()));
    }
    if (known_paths_location_ != location_) {
      // Different folder (or first load): no "new" stickers for the initial set.
      model_->clear_new_marks();
    } else if (!known_paths_.empty()) {
      for (const QString& p : next_paths) {
        if (!known_paths_.contains(p)) {
          model_->mark_new(p);
        }
      }
      // Drop thumbs for paths that vanished; drop "new" marks for those too.
      for (const QString& p : known_paths_) {
        if (!next_paths.contains(p)) {
          model_->clear_thumbnail(p);
        }
      }
      model_->prune_new_marks(next_paths);
    }
    known_paths_ = next_paths;
    known_paths_location_ = location_;
  }
  collection_.sorter().set_ascending(sort_ascending_);
  const bool soft = soft_directory_reload_;
  soft_directory_reload_ = false;
  // Soft watcher refresh: merge into existing collection (preserve order of
  // survivors, append newcomers) instead of replacing the whole vector.
  const bool content_filter =
      filter_edit_ != nullptr && !filter_edit_->text().isEmpty()
      && filter_expression_needs_content_io(filter_edit_->text());
  if (soft && !collection_.empty()) {
    // Avoid rebuild_visible when content matchers would hit the GUI thread.
    collection_.merge_items(std::move(items), !content_filter);
  } else {
    collection_.set_items_unsorted(std::move(items));
  }
  if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()) {
    if (content_filter) {
      // Keep previous visible list until FilterWorker finishes (no empty flash).
      request_async_filter(/*keep_previous_visible=*/true);
    } else {
      collection_.set_name_filter(filter_edit_->text().toStdString());
    }
  }
  // Soft (watcher) reloads: keep the previous paint until sort finishes to avoid
  // unsorted→sorted double flicker. Hard navigation still paints ASAP.
  if (!soft) {
    refresh_list();
  }
  set_status(QStringLiteral("%1 items").arg(
      soft ? known_paths_.size() : collection_.visible_items().size()));
  // After soft path, visible may still be old until sort/filter apply; status updated again later.
  // Content filters own the visible list via FilterWorker; replace_items_sorted would
  // rebuild_visible with matchers (GUI I/O) or wipe the async filter result.
  if (!content_filter) {
    request_async_sort();
  }
  request_thumbnails_for_visible();
}

void MainWindow::update_detail_row_heights()
{
  if (tree_view_ == nullptr || model_ == nullptr) {
    return;
  }
  const bool variable =
      collection_.group_mode() != collection::GroupMode::None || model_->show_timegaps();
  tree_view_->setUniformRowHeights(!variable);
  tree_view_->doItemsLayout();
}

void MainWindow::request_async_sort()
{
  // Content filters own visible_ via FilterWorker. Sorting must not call
  // replace_items_sorted → rebuild_visible (GUI content I/O / wipe filter).
  if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()
      && filter_expression_needs_content_io(filter_edit_->text())) {
    collection_.sort_items_only();
    request_async_filter(/*keep_previous_visible=*/true);
    return;
  }
  if (sort_worker_ == nullptr) {
    collection_.apply_sort();
    refresh_list();
    return;
  }
  const quint64 gen = ++sort_generation_;
  auto items = collection_.items(); // copy
  const auto key = collection_.sorter().key();
  const bool asc = collection_.sorter().ascending();
  const bool dirs_first = collection_.sorter().directories_first();
  QMetaObject::invokeMethod(sort_worker_, "sort_items", Qt::QueuedConnection,
                            Q_ARG(std::vector<dirtoo::fs::FileInfo>, items),
                            Q_ARG(dirtoo::collection::SortKey, key), Q_ARG(bool, asc),
                            Q_ARG(bool, dirs_first), Q_ARG(quint64, gen));
}

void MainWindow::on_sort_finished(quint64 generation, std::vector<fs::FileInfo> items)
{
  if (generation != sort_generation_ || search_active_) {
    return;
  }
  // match_/filter and show_hidden stay; only item order changes.
  collection_.replace_items_sorted(std::move(items));
  refresh_list();
  set_status(QStringLiteral("%1 items").arg(collection_.visible_items().size()));
  request_thumbnails_for_visible();
}

void MainWindow::on_directory_load_failed(quint64 generation, QString error)
{
  qWarning().noquote() << QStringLiteral("directory_load_failed gen=%1: %2")
                              .arg(generation)
                              .arg(error);

  if (generation != dir_load_generation_) {
    return;
  }
  set_status(error);
  if (message_area_ != nullptr) {
    message_area_->show_error(error);
  }
}

void MainWindow::on_show_filter_help()
{
  auto* dlg = new QDialog(this);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setWindowTitle(QStringLiteral("Filter expression help"));
  dlg->resize(560, 720);
  auto* v = new QVBoxLayout(dlg);
  auto* browser = new QTextBrowser(dlg);
  browser->setOpenExternalLinks(false);
  browser->setHtml(QString::fromStdString(dirtoo::filter::filter_help_html()));
  v->addWidget(browser, 1);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
  connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
  // Close button maps to rejected for Close-only box
  connect(buttons, &QDialogButtonBox::clicked, dlg, [dlg](QAbstractButton*) { dlg->close(); });
  v->addWidget(buttons);
  dlg->show();
}

void MainWindow::update_filter_chrome(bool filtered)
{
  // Tint only the file views when a filter is active (dirtoo-py set_filtered).
  // Keep the filter bar on the window/chrome palette so it does not pick up the
  // view Base color (which made the bar look “merged” into the list).
  const QColor tint(220, 220, 255);
  const QColor app_base = qApp->palette().color(QPalette::Base);
  const QColor app_window = qApp->palette().color(QPalette::Window);

  auto apply_view_bg = [&](QWidget* w) {
    if (w == nullptr) {
      return;
    }
    QPalette pal = w->palette();
    pal.setColor(QPalette::Base, filtered ? tint : app_base);
    w->setPalette(pal);
  };
  apply_view_bg(tree_view_);
  apply_view_bg(icon_view_);
  if (graphics_view_ != nullptr) {
    if (filtered) {
      graphics_view_->setBackgroundBrush(QBrush(tint));
    } else {
      graphics_view_->setBackgroundBrush(QBrush(app_base));
    }
  }

  if (filter_row_ != nullptr) {
    // Explicit chrome background; never inherit the tinted view Base.
    filter_row_->setAutoFillBackground(true);
    QPalette bar_pal = filter_row_->palette();
    bar_pal.setColor(QPalette::Window, app_window);
    bar_pal.setColor(QPalette::Base, app_base);
    filter_row_->setPalette(bar_pal);
    filter_row_->setStyleSheet(QString());
  }
  if (filter_edit_ != nullptr) {
    QPalette edit_pal = filter_edit_->palette();
    edit_pal.setColor(QPalette::Base, app_base);
    edit_pal.setColor(QPalette::Window, app_base);
    filter_edit_->setPalette(edit_pal);
  }
}

void MainWindow::on_filter_changed(const QString& text)
{
  update_filter_chrome(!text.isEmpty());
  if (filter_expression_needs_content_io(text)) {
    // Content predicates must not run on the GUI thread.
    request_async_filter();
    return;
  }
  collection_.set_name_filter(text.toStdString());
  refresh_list();
  request_thumbnails_for_visible();
  if (!text.isEmpty() && !collection_.filter_parse_ok()) {
    if (message_area_ != nullptr) {
      message_area_->show_info(QStringLiteral("Filter parse issue — using substring fallback"));
    }
  }
}

void MainWindow::request_async_filter(bool keep_previous_visible)
{
  if (filter_worker_ == nullptr) {
    collection_.set_name_filter(
        filter_edit_ != nullptr ? filter_edit_->text().toStdString() : std::string{});
    refresh_list();
    return;
  }
  const quint64 gen = ++filter_generation_;
  set_status(QStringLiteral("Filtering…"));
  auto items = collection_.items();
  const QString expr = filter_edit_ != nullptr ? filter_edit_->text() : QString();
  const bool show_hidden = collection_.show_hidden();
  const auto group_mode = collection_.group_mode();
  // Avoid GUI-thread content I/O. For interactive filter changes, clear visible
  // until the worker finishes; for soft watcher reloads keep the previous list.
  if (!keep_previous_visible) {
    collection_.replace_visible({}, true);
    refresh_list();
  }
  QMetaObject::invokeMethod(
      filter_worker_, "filter_items", Qt::QueuedConnection,
      Q_ARG(std::vector<dirtoo::fs::FileInfo>, items), Q_ARG(QString, expr),
      Q_ARG(bool, show_hidden), Q_ARG(dirtoo::collection::GroupMode, group_mode),
      Q_ARG(quint64, gen));
}

void MainWindow::on_filter_finished(quint64 generation, std::vector<dirtoo::fs::FileInfo> visible,
                                    bool parse_ok)
{
  if (generation != filter_generation_ || search_active_) {
    return;
  }
  // Apply current sort to the filtered list (in-memory only; no FS I/O).
  collection_.sorter().sort(visible);
  collection_.replace_visible(std::move(visible), parse_ok);
  refresh_list();
  request_thumbnails_for_visible();
  if (!parse_ok && message_area_ != nullptr) {
    message_area_->show_info(QStringLiteral("Filter parse issue — using substring fallback"));
  }
  set_status(QStringLiteral("%1 items").arg(collection_.visible_items().size()));
}

void MainWindow::apply_sort_key(collection::SortKey key, bool toggle_if_same)
{
  if (toggle_if_same && sort_key_ == key) {
    sort_ascending_ = !sort_ascending_;
  } else {
    sort_key_ = key;
    sort_ascending_ = true;
  }
  collection_.sorter().set_key(key);
  collection_.sorter().set_ascending(sort_ascending_);
  update_sort_toolbar_label();
  if (tree_view_ != nullptr && tree_view_->header() != nullptr) {
    tree_view_->header()->setSortIndicatorShown(true);
    // Map SortKey → visible FileListColumn for indicator when possible.
    int section = static_cast<int>(FileListColumn::Name);
    switch (key) {
    case collection::SortKey::Size:
      section = static_cast<int>(FileListColumn::Size);
      break;
    case collection::SortKey::Width:
      section = static_cast<int>(FileListColumn::Width);
      break;
    case collection::SortKey::Height:
      section = static_cast<int>(FileListColumn::Height);
      break;
    case collection::SortKey::Resolution:
      section = static_cast<int>(FileListColumn::Dimensions);
      break;
    case collection::SortKey::AspectRatio:
      section = static_cast<int>(FileListColumn::AspectRatio);
      break;
    case collection::SortKey::Framerate:
      section = static_cast<int>(FileListColumn::Framerate);
      break;
    case collection::SortKey::Duration:
      section = static_cast<int>(FileListColumn::Duration);
      break;
    case collection::SortKey::Modified:
      section = static_cast<int>(FileListColumn::Modified);
      break;
    case collection::SortKey::Type:
    case collection::SortKey::Extension:
      section = static_cast<int>(FileListColumn::Type);
      break;
    default:
      section = static_cast<int>(FileListColumn::Name);
      break;
    }
    tree_view_->header()->setSortIndicator(
        section, sort_ascending_ ? Qt::AscendingOrder : Qt::DescendingOrder);
  }
  request_async_sort();
}

void MainWindow::update_sort_toolbar_label()
{
  if (sort_toolbar_btn_ == nullptr) {
    return;
  }
  QString title = QStringLiteral("Name");
  switch (sort_key_) {
  case collection::SortKey::Size:
    title = QStringLiteral("Size");
    break;
  case collection::SortKey::Modified:
    title = QStringLiteral("Modified");
    break;
  case collection::SortKey::Type:
  case collection::SortKey::Extension:
    title = QStringLiteral("Type");
    break;
  case collection::SortKey::Width:
    title = QStringLiteral("Width");
    break;
  case collection::SortKey::Height:
    title = QStringLiteral("Height");
    break;
  case collection::SortKey::Resolution:
    title = QStringLiteral("Dimensions");
    break;
  case collection::SortKey::AspectRatio:
    title = QStringLiteral("Aspect");
    break;
  case collection::SortKey::Duration:
    title = QStringLiteral("Duration");
    break;
  case collection::SortKey::Framerate:
    title = QStringLiteral("FPS");
    break;
  case collection::SortKey::Random:
    title = QStringLiteral("Random");
    break;
  default:
    title = QStringLiteral("Name");
    break;
  }
  sort_toolbar_btn_->setText(title);
}

void MainWindow::on_header_clicked(int section)
{
  using collection::SortKey;
  SortKey key = SortKey::Name;
  switch (static_cast<FileListColumn>(section)) {
  case FileListColumn::Name:
    key = SortKey::Name;
    break;
  case FileListColumn::Size:
    key = SortKey::Size;
    break;
  case FileListColumn::Width:
    key = SortKey::Width;
    break;
  case FileListColumn::Height:
    key = SortKey::Height;
    break;
  case FileListColumn::Dimensions:
    key = SortKey::Resolution; // width * height
    break;
  case FileListColumn::AspectRatio:
    key = SortKey::AspectRatio;
    break;
  case FileListColumn::Framerate:
    key = SortKey::Framerate;
    break;
  case FileListColumn::Duration:
    key = SortKey::Duration;
    break;
  case FileListColumn::Modified:
    key = SortKey::Modified;
    break;
  case FileListColumn::Type:
    key = SortKey::Type;
    break;
  case FileListColumn::Count:
    return;
  }
  apply_sort_key(key, /*toggle_if_same=*/true);
}

void MainWindow::on_item_activated(const QModelIndex& index)
{
  const fs::FileInfo* fi = model_->file_at(index.row());
  if (fi == nullptr) {
    return;
  }
  if (fi->is_directory()) {
    if (location_.is_archive()) {
      open_location(location_.join(fi->basename()));
    } else {
      open_location(fi->location());
    }
  } else if (location_.is_archive()) {
    // Extract single member then open with the default application.
    const auto member = location_.entry_path().empty()
                            ? std::filesystem::path{fi->basename()}
                            : location_.entry_path() / fi->basename();
    const auto cache = std::filesystem::temp_directory_path() / "dirtoo-open" /
                       std::to_string(std::hash<std::string>{}(location_.as_path().string()));
    auto extracted = archive::extract_member(location_.as_path(), member, cache);
    if (!extracted) {
      QMessageBox::warning(this, QStringLiteral("Archive"),
                           QString::fromStdString(extracted.error()));
      return;
    }
    if (fs::looks_like_archive(*extracted)) {
      open_location(fs::Location::from_archive(*extracted, {}));
    } else {
      open_default(*extracted);
    }
  } else if (fs::looks_like_archive(fi->path())) {
    open_location(fs::Location::from_archive(fi->path(), {}));
  } else {
    open_default(fi->path());
  }
}

std::vector<fs::FileInfo> MainWindow::selected_fileinfos() const
{
  if (model_ == nullptr) {
    return {};
  }
  if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
    std::vector<fs::FileInfo> out;
    for (int row : graphics_view_->selected_rows()) {
      if (const auto* fi = model_->file_at(row)) {
        out.push_back(*fi);
      }
    }
    return out;
  }
  auto* view = current_view();
  if (view == nullptr || view->selectionModel() == nullptr) {
    return {};
  }
  return model_->files_at(view->selectionModel()->selectedIndexes());
}

void MainWindow::on_context_menu(const QPoint& pos)
{
  auto* view = current_view();
  const bool graphics = (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr);
  if (view == nullptr && !graphics) {
    return;
  }

  // Resolve the item under the pointer (if any). Empty background → directory menu.
  QModelIndex under;
  if (graphics && graphics_view_ != nullptr) {
    under = graphics_view_->index_at(pos);
    if (under.isValid()) {
      const auto rows = graphics_view_->selected_rows();
      bool already = false;
      for (int r : rows) {
        if (r == under.row()) {
          already = true;
          break;
        }
      }
      if (!already) {
        graphics_view_->select_row(under.row(), true);
      }
    }
  } else if (view != nullptr && view->selectionModel() != nullptr) {
    const QPoint vp = view->viewport()->mapFrom(view, pos);
    under = view->indexAt(vp);
    if (under.isValid() && !view->selectionModel()->isSelected(under)) {
      view->selectionModel()->select(
          under, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
      view->setCurrentIndex(under);
    }
  }

  FileContextMenuCallbacks cb;
  cb.current_location = location_;
  cb.mkdir = [this] { on_mkdir(); };
  cb.create_file = [this] { on_create_file(); };
  cb.paste = [this] { on_paste(); };
  cb.select_all = [this] { on_select_all(); };
  cb.cut = [this] { on_cut(); };
  cb.copy = [this] { on_copy(); };
  cb.delete_selected = [this] { on_delete_selected(); };
  cb.rename_selected = [this] { on_rename_selected(); };
  cb.properties_selected = [this] { on_properties(); };
  cb.reload_thumbnails = [this] { on_reload_thumbnails(); };
  cb.prepare_thumbnails = [this] { on_prepare_thumbnails(); };
  cb.make_directory_thumbnails = [this] { on_make_directory_thumbnails(); };
  cb.open_location = [this](const fs::Location& loc) { open_location(loc); };
  cb.open_location_new_window = [this](const fs::Location& loc) {
    auto* win = new MainWindow();
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->show();
    win->open_location(loc);
  };
  cb.open_terminal = [this](const std::filesystem::path& dir) {
    if (!open_in_terminal(dir)) {
      set_status(QStringLiteral("Could not launch a terminal emulator"));
    }
  };
  cb.paste_into = [this](const std::filesystem::path& dest) {
    const ClipboardPayload payload =
        parse_clipboard_mime(QApplication::clipboard()->mimeData());
    if (payload.paths.empty()) {
      set_status(QStringLiteral("Clipboard has no files"));
      return;
    }
    TransferRequest req;
    req.mode = payload.mode;
    req.sources = payload.paths;
    req.destination_directory = dest;
    start_transfer(req);
  };
  cb.set_status = [this](const QString& s) { set_status(s); };
  cb.show_properties = [this](const std::vector<fs::FileInfo>& items) {
    show_properties_dialog(this, items);
  };

  const QPoint global = graphics ? graphics_view_->mapToGlobal(pos)
                                 : view->viewport()->mapToGlobal(pos);
  if (!under.isValid()) {
    exec_directory_context_menu(this, global, cb);
  } else {
    exec_item_context_menu(this, global, selected_fileinfos(), cb);
  }
}

void MainWindow::set_clipboard(ClipboardMode mode)
{
  const auto selected = selected_fileinfos();
  if (selected.empty()) {
    set_status(QStringLiteral("Nothing selected"));
    return;
  }
  std::vector<std::filesystem::path> paths;
  paths.reserve(selected.size());
  for (const auto& fi : selected) {
    paths.push_back(fi.path());
  }
  QApplication::clipboard()->setMimeData(make_clipboard_mime(mode, paths));
  QString verb = QStringLiteral("copied");
  if (mode == ClipboardMode::Cut) {
    verb = QStringLiteral("cut");
  } else if (mode == ClipboardMode::Link) {
    verb = QStringLiteral("marked for link");
  }
  set_status(QStringLiteral("%1 item(s) %2").arg(paths.size()).arg(verb));
  update_edit_actions();
}

void MainWindow::on_copy()
{
  set_clipboard(ClipboardMode::Copy);
}

void MainWindow::on_cut()
{
  set_clipboard(ClipboardMode::Cut);
}

void MainWindow::on_paste()
{
  if (location_.is_archive()) {
    set_status(QStringLiteral("Read-only: browsing inside an archive"));
    return;
  }

  if (transfer_controller_.busy()) {
    return;
  }

  const ClipboardPayload payload = parse_clipboard_mime(QApplication::clipboard()->mimeData());
  if (payload.paths.empty()) {
    set_status(QStringLiteral("Clipboard has no files"));
    return;
  }

  if (payload.mode == ClipboardMode::Link) {
    on_paste_link();
    return;
  }

  TransferRequest req;
  req.mode = payload.mode;
  req.destination_directory = location_.as_path();
  req.sources = payload.paths;
  start_transfer(req);
}

void MainWindow::on_paste_link()
{
  if (location_.is_archive()) {
    set_status(QStringLiteral("Read-only: browsing inside an archive"));
    return;
  }
  const ClipboardPayload payload = parse_clipboard_mime(QApplication::clipboard()->mimeData());
  if (payload.paths.empty()) {
    // Allow "Paste as Link" using whatever paths are on the clipboard.
    set_status(QStringLiteral("Clipboard has no files"));
    return;
  }
  int ok = 0;
  int fail = 0;
  for (const auto& src : payload.paths) {
    const auto dest = location_.as_path() / src.filename();
    auto result = dirops::create_symlink(src, dest);
    if (result) {
      ++ok;
      operations_history().record_simple(OperationKind::Symlink, {src}, dest, true);
    } else {
      ++fail;
      operations_history().record_simple(OperationKind::Symlink, {src}, dest, false,
                                         QString::fromStdString(result.error().to_string()));
      if (message_area_ != nullptr) {
        message_area_->show_error(QString::fromStdString(result.error().to_string()));
      }
    }
  }
  set_status(QStringLiteral("Linked %1 (%2 failed)").arg(ok).arg(fail));
  on_directory_changed();
}

void MainWindow::on_mkdir()
{
  if (location_.is_archive()) {
    set_status(QStringLiteral("Read-only: browsing inside an archive"));
    return;
  }

  const auto name_opt = ask_item_name(this, QStringLiteral("New Folder"),
                                      QStringLiteral("Folder name:"),
                                      QStringLiteral("New Folder"),
                                      QStringLiteral("Create"));
  if (!name_opt || name_opt->isEmpty()) {
    return;
  }
  const QString name = *name_opt;

  const auto dest = location_.as_path() / name.toStdString();
  if (std::filesystem::exists(dest)) {
    const auto chosen = ask_conflict_policy(this, name);
    if (!chosen || chosen->policy == dirops::ConflictPolicy::Skip) {
      return;
    }
    if (chosen->policy == dirops::ConflictPolicy::Overwrite) {
      auto rm = dirops::remove_path(dest);
      if (!rm) {
        QMessageBox::warning(this, QStringLiteral("New Folder"),
                             QString::fromStdString(rm.error().to_string()));
        return;
      }
    } else if (chosen->policy == dirops::ConflictPolicy::Rename) {
      const auto unique =
          dest.parent_path() / (dest.stem().string() + " (2)" + dest.extension().string());
      auto result = dirops::create_directory(unique);
      if (!result) {
        QMessageBox::warning(this, QStringLiteral("New Folder"),
                             QString::fromStdString(result.error().to_string()));
      }
      on_directory_changed();
      return;
    }
  }

  auto result = dirops::create_directory(dest);
  if (!result) {
    operations_history().record_simple(OperationKind::Mkdir, {}, dest, false,
                                       QString::fromStdString(result.error().to_string()));
    QMessageBox::warning(this, QStringLiteral("New Folder"),
                         QString::fromStdString(result.error().to_string()));
    return;
  }
  operations_history().record_simple(OperationKind::Mkdir, {}, dest, true);
  on_directory_changed();
}

void MainWindow::on_create_file()
{
  if (location_.is_archive()) {
    set_status(QStringLiteral("Read-only: browsing inside an archive"));
    return;
  }

  const auto name_opt = ask_item_name(this, QStringLiteral("New File"),
                                      QStringLiteral("File name:"),
                                      QStringLiteral("New File"),
                                      QStringLiteral("Create"));
  if (!name_opt || name_opt->isEmpty()) {
    return;
  }
  const QString name = *name_opt;
  auto dest = location_.as_path() / name.toStdString();
  if (std::filesystem::exists(dest)) {
    const auto unique = dirops::unique_path(dest);
    dest = unique;
  }
  auto result = dirops::create_file(dest);
  if (!result) {
    operations_history().record_simple(OperationKind::Mkfile, {}, dest, false,
                                       QString::fromStdString(result.error().to_string()));
    QMessageBox::warning(this, QStringLiteral("New File"),
                         QString::fromStdString(result.error().to_string()));
    return;
  }
  operations_history().record_simple(OperationKind::Mkfile, {}, dest, true);
  on_directory_changed();
}


void MainWindow::on_swap_names()
{
  if (location_.is_archive()) {
    set_status(QStringLiteral("Read-only: browsing inside an archive"));
    return;
  }
  const auto selected = selected_fileinfos();
  if (selected.size() != 2) {
    set_status(QStringLiteral("Select exactly two items to swap names"));
    return;
  }
  auto result = dirops::swap_names(selected[0].path(), selected[1].path());
  if (!result) {
    operations_history().record_simple(
        OperationKind::Swap, {selected[0].path(), selected[1].path()}, {}, false,
        QString::fromStdString(result.error().to_string()));
    QMessageBox::warning(this, QStringLiteral("Swap Names"),
                         QString::fromStdString(result.error().to_string()));
    return;
  }
  operations_history().record_simple(OperationKind::Swap,
                                     {selected[0].path(), selected[1].path()}, {}, true);
  on_directory_changed();
}

void MainWindow::on_toggle_show_abspath(bool checked)
{
  show_abspath_ = checked;
  if (model_ != nullptr) {
    model_->set_show_abspath(checked);
  }
  if (graphics_view_ != nullptr) {
    graphics_view_->viewport()->update();
  }
}

void MainWindow::on_rename_selected()
{
  if (location_.is_archive()) {
    set_status(QStringLiteral("Read-only: browsing inside an archive"));
    return;
  }

  const auto selected = selected_fileinfos();
  if (selected.size() != 1) {
    set_status(QStringLiteral("Select exactly one item to rename"));
    return;
  }

  const auto& fi = selected.front();
  const auto name_opt = ask_item_name(this, QStringLiteral("Rename"),
                                      QStringLiteral("New name:"),
                                      QString::fromStdString(fi.basename()),
                                      QStringLiteral("Rename"));
  if (!name_opt || name_opt->isEmpty()) {
    return;
  }
  const QString name = *name_opt;

  const auto dest = fi.path().parent_path() / name.toStdString();
  dirops::Options opt;
  if (std::filesystem::exists(dest) && dest != fi.path()) {
    const auto chosen = ask_conflict_policy(this, name);
    if (!chosen) {
      return;
    }
    opt.conflict = chosen->policy;
  }

  auto result = dirops::rename_path(fi.path(), dest, opt);
  if (!result) {
    operations_history().record_simple(OperationKind::Rename, {fi.path()}, dest, false,
                                       QString::fromStdString(result.error().to_string()));
    QMessageBox::warning(this, QStringLiteral("Rename"),
                         QString::fromStdString(result.error().to_string()));
    return;
  }
  operations_history().record_simple(OperationKind::Rename, {fi.path()}, dest, true);
  on_directory_changed();
}

void MainWindow::on_delete_selected()
{
  if (location_.is_archive()) {
    set_status(QStringLiteral("Read-only: browsing inside an archive"));
    return;
  }

  const auto selected = selected_fileinfos();
  if (selected.empty()) {
    return;
  }

  const QString msg = selected.size() == 1
                          ? QStringLiteral("Delete “%1”?")
                                .arg(QString::fromStdString(selected.front().basename()))
                          : QStringLiteral("Delete %1 items?").arg(selected.size());
  if (QMessageBox::question(this, QStringLiteral("Delete"), msg) != QMessageBox::Yes) {
    return;
  }

  for (const auto& fi : selected) {
    auto result = dirops::remove_path(fi.path());
    if (!result) {
      operations_history().record_simple(OperationKind::Delete, {fi.path()}, {}, false,
                                         QString::fromStdString(result.error().to_string()));
      QMessageBox::warning(this, QStringLiteral("Delete"),
                           QString::fromStdString(result.error().to_string()));
      break;
    }
    operations_history().record_simple(OperationKind::Delete, {fi.path()}, {}, true);
  }
  on_directory_changed();
}

void MainWindow::refresh_list()
{
  model_->refresh();
  update_status_selection();
}


void MainWindow::on_properties()
{
  const auto selected = selected_fileinfos();
  if (selected.empty()) {
    set_status(QStringLiteral("Nothing selected"));
    return;
  }
  show_properties_dialog(this, selected);
}

void MainWindow::on_selection_changed()
{
  update_status_selection();
}

void MainWindow::update_status_selection()
{
  // Left: filename (or multi-selection summary). Right: directory size stats
  // (sum of listed item sizes — same idea as dirtoo-py controller._update_info).
  const auto& all = collection_.items();
  const auto& visible = collection_.visible_items();
  const auto selected = selected_fileinfos();

  std::uint64_t visible_bytes = 0;
  for (const auto& fi : visible) {
    visible_bytes += fi.size();
  }
  std::uint64_t total_bytes = 0;
  for (const auto& fi : all) {
    total_bytes += fi.size();
  }

  QString info = QStringLiteral("%1 visible (%2)")
                     .arg(visible.size())
                     .arg(format_byte_size(visible_bytes));
  if (all.size() != visible.size()) {
    info += QStringLiteral(", %1 total (%2)")
                .arg(all.size())
                .arg(format_byte_size(total_bytes));
  }

  if (!selected.empty()) {
    std::uint64_t selected_bytes = 0;
    for (const auto& fi : selected) {
      selected_bytes += fi.size();
    }
    info += QStringLiteral(", %1 selected (%2)")
                .arg(selected.size())
                .arg(format_byte_size(selected_bytes));
  }

  if (status_info_label_ != nullptr) {
    status_info_label_->setText(QStringLiteral("  ") + info);
  }

  if (selected.empty()) {
    // Clear left so transient set_status messages (Loading…, etc.) can show;
    // steady state with no selection leaves the right panel as the summary.
    if (status_label_ != nullptr) {
      status_label_->setText(QString());
    }
  } else if (selected.size() == 1) {
    set_status(QString::fromStdString(selected.front().path().string()));
  } else {
    set_status(QStringLiteral("%1 selected").arg(selected.size()));
  }
}

void MainWindow::on_urls_dropped(const QList<QUrl>& urls, Qt::DropAction action)
{
  on_urls_dropped_to(urls, action, {});
}

void MainWindow::on_select_all()
{
  if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
    // Must mark all model rows, not only live viewport tiles.
    graphics_view_->select_all();
    return;
  }
  if (QAbstractItemView* view = current_view()) {
    view->selectAll();
  }
}

void MainWindow::on_save_file_list()
{
  if (model_ == nullptr) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Save File List As"),
      QString::fromStdString(location_.as_path().string()) + QStringLiteral("/filelist.txt"),
      QStringLiteral("Text files (*.txt);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
    QMessageBox::warning(this, QStringLiteral("Save File List"),
                         QStringLiteral("Could not write %1").arg(path));
    return;
  }
  QTextStream out(&file);
  const int rows = model_->rowCount();
  for (int r = 0; r < rows; ++r) {
    if (const auto* fi = model_->file_at(r)) {
      out << QString::fromStdString(fi->path().string()) << QChar('\n');
    }
  }
  set_status(QStringLiteral("Saved %1 paths to %2").arg(rows).arg(path));
}

void MainWindow::showEvent(QShowEvent* event)
{
  QMainWindow::showEvent(event);
  // Tool buttons exist after the toolbar is realized.
  if (parent_act_ != nullptr) {
    for (auto* tb : findChildren<QToolButton*>()) {
      if (tb->defaultAction() == parent_act_) {
        tb->installEventFilter(this);
      }
    }
  }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
  persist_settings();
  QMainWindow::closeEvent(event);
}


void MainWindow::on_refresh()
{
  on_directory_changed();
  set_status(QStringLiteral("Refreshed"));
}

void MainWindow::on_focus_location()
{
  show_location_line_edit();
  location_edit_->setFocus(Qt::ShortcutFocusReason);
  location_edit_->selectAll();
}


MainWindow* MainWindow::open_new_window(const fs::Location& location)
{
  auto* win = new MainWindow;
  win->setAttribute(Qt::WA_DeleteOnClose);
  win->open_location(location);
  win->show();
  win->raise();
  win->activateWindow();
  return win;
}

void MainWindow::on_new_window()
{
  open_new_window(location_);
}

void MainWindow::on_breadcrumb_location_new_window(const fs::Location& location)
{
  open_new_window(location);
}

void MainWindow::on_breadcrumb_location(const fs::Location& location)
{
  open_location(location);
}

void MainWindow::on_location_edit_requested()
{
  show_location_line_edit();
  location_edit_->setFocus(Qt::MouseFocusReason);
  location_edit_->selectAll();
}

void MainWindow::show_location_buttons()
{
  if (location_buttons_ != nullptr) {
    location_buttons_->show();
  }
  if (location_edit_ != nullptr) {
    location_edit_->hide();
  }
}

void MainWindow::show_location_line_edit()
{
  if (location_buttons_ != nullptr) {
    location_buttons_->hide();
  }
  if (location_edit_ != nullptr) {
    location_edit_->show();
  }
}

void MainWindow::on_open_with()
{
  const auto selected = selected_fileinfos();
  if (selected.empty()) {
    set_status(QStringLiteral("Nothing selected"));
    return;
  }
  std::vector<std::filesystem::path> paths;
  for (const auto& fi : selected) {
    paths.push_back(fi.path());
  }
  // Prefer a listed default app when available; otherwise prompt for a command.
  QMimeDatabase db;
  const QMimeType mt = db.mimeTypeForFile(QString::fromStdString(paths.front().string()),
                                          QMimeDatabase::MatchExtension);
  const auto apps = apps_for_mime(mt.isValid() ? mt.name() : QStringLiteral("application/octet-stream"));
  if (!apps.empty()) {
    if (launch_desktop_app(apps.front(), paths)) {
      return;
    }
  }
  open_with_command_dialog(this, paths);
}

void MainWindow::on_open_terminal()
{
  std::filesystem::path dir = location_.as_path();
  const auto selected = selected_fileinfos();
  if (selected.size() == 1 && selected.front().is_directory()) {
    dir = selected.front().path();
  }
  if (!open_in_terminal(dir)) {
    set_status(QStringLiteral("Could not launch a terminal emulator"));
  }
}


void MainWindow::on_toggle_hidden(bool checked)
{
  const QString expr = filter_edit_ != nullptr ? filter_edit_->text() : QString();
  if (filter_expression_needs_content_io(expr)) {
    collection_.set_show_hidden(checked, false);
  if (directory_tree_model_ != nullptr) {
    directory_tree_model_->set_show_hidden(checked);
  }
    request_async_filter();
    return;
  }
  collection_.set_show_hidden(checked);
  if (directory_tree_model_ != nullptr) {
    directory_tree_model_->set_show_hidden(checked);
  }
  refresh_list();
  request_thumbnails_for_visible();
}


void MainWindow::on_toggle_filter_visible()
{
  if (show_filter_act_ == nullptr) {
    return;
  }
  show_filter_act_->toggle();
}


void MainWindow::on_location_text_edited(const QString& text)
{
  path_completion_pending_ = text;
  if (path_completion_timer_ != nullptr) {
    path_completion_timer_->start();
  }
}

void MainWindow::on_path_completion_timeout()
{
  if (path_completion_worker_ == nullptr || path_completion_thread_ == nullptr) {
    return;
  }
  const QString text = path_completion_pending_;
  if (text.isEmpty()) {
    if (path_completion_model_ != nullptr) {
      path_completion_model_->setStringList({});
    }
    return;
  }
  const quint64 id = ++path_completion_request_id_;
  QMetaObject::invokeMethod(
      path_completion_worker_,
      [worker = path_completion_worker_, id, text] { worker->complete(id, text); },
      Qt::QueuedConnection);
}

void MainWindow::on_path_completions_ready(quint64 request_id, const QString& longest,
                                           const QStringList& candidates)
{
  (void)longest;
  if (request_id != path_completion_request_id_) {
    return; // stale
  }
  if (path_completion_model_ == nullptr) {
    return;
  }
  path_completion_model_->setStringList(candidates);
  if (path_completer_ != nullptr && location_edit_ != nullptr && location_edit_->hasFocus()
      && !candidates.isEmpty()) {
    path_completer_->complete();
  }
}


void MainWindow::on_show_search()
{
  stop_search();
  if (search_edit_ == nullptr) {
    return;
  }
  if (search_row_ != nullptr) {
    search_row_->setVisible(true);
  } else if (search_edit_ != nullptr) {
    search_edit_->setVisible(true);
  }
  search_edit_->setFocus(Qt::ShortcutFocusReason);
  search_edit_->selectAll();
}

void MainWindow::stop_search()
{
  search_controller_.stop();
  search_batch_.clear();
}


void MainWindow::on_search_submitted()
{
  if (search_edit_ == nullptr) {
    return;
  }
  const QString expr = search_edit_->text().trimmed();
  if (expr.isEmpty()) {
    return;
  }
  if (location_.is_archive()) {
    set_status(QStringLiteral("Recursive search is not available inside archives"));
    return;
  }

  stop_search();
  search_results_.clear();
  search_batch_.clear();
  search_status_matched_ = 0;
  search_active_ = true;
  collection_.clear();
  collection_.clear_filter();
  refresh_list();

  const QString root = QString::fromStdString(location_.as_path().string());
  const bool show_hidden = show_hidden_act_ != nullptr && show_hidden_act_->isChecked();
  set_status(QStringLiteral("Searching…"));
  if (message_area_ != nullptr) {
    message_area_->show_info(QStringLiteral("Recursive search: %1").arg(expr));
  }
  search_controller_.start(root, expr, show_hidden, /*max_depth=*/-1);
}

void MainWindow::on_search_match(const QString& path, bool is_directory, quint64 size)
{
  if (!search_active_) {
    return;
  }
  // Synthetic only: avoid FileInfo::from_path (stat) on the GUI thread for every hit.
  const std::filesystem::path p{path.toStdString()};
  auto info = fs::FileInfo::synthetic(fs::Location::from_path(p), p.filename().string(),
                                      is_directory, size);
  search_results_.push_back(info);
  search_batch_.push_back(std::move(info));
  // Incremental inserts (batched) beat periodic full layoutChanged on large sets.
  // Early results flush soon for responsiveness; larger batches later reduce model churn.
  if (search_batch_.size() >= 48 || search_results_.size() < 24) {
    flush_search_batch();
  } else if (search_batch_.size() == 1) {
    // Schedule a deferred flush so a trickle of hits still paints without waiting
    // for a full batch (or search end).
    QTimer::singleShot(50, this, [this] {
      if (search_active_) {
        flush_search_batch();
      }
    });
  }
}

void MainWindow::flush_search_batch()
{
  if (search_batch_.empty()) {
    return;
  }
  const int count = static_cast<int>(search_batch_.size());
  collection_.append_visible_items(std::move(search_batch_));
  search_batch_.clear();
  if (model_ != nullptr) {
    model_->notify_rows_appended(count);
  } else {
    refresh_list();
  }
  update_status_selection();
}

void MainWindow::on_search_progress(quint64 visited, quint64 matched)
{
  (void)visited;
  // Throttle status updates — progress can fire very frequently on huge trees.
  if (matched == search_status_matched_) {
    return;
  }
  if (matched < 32 || matched - search_status_matched_ >= 32) {
    search_status_matched_ = matched;
    set_status(QStringLiteral("Searching… %1 matches").arg(matched));
  }
}

void MainWindow::on_search_finished(quint64 matched, quint64 visited, const QString& error)
{
  flush_search_batch();
  request_thumbnails_for_visible();
  if (!error.isEmpty() && error != QStringLiteral("cancelled")) {
    set_status(error);
    if (message_area_ != nullptr) {
      message_area_->show_info(error);
    }
  } else if (error == QStringLiteral("cancelled")) {
    set_status(
        QStringLiteral("Search cancelled — %1 matches (%2 visited)").arg(matched).arg(visited));
  } else {
    set_status(
        QStringLiteral("Search done — %1 matches (%2 visited)").arg(matched).arg(visited));
  }
  // Thread lifecycle owned by SearchController.
  // Keep search_active_ true so directory watcher does not wipe results until user navigates.
}

void MainWindow::on_about()
{
  show_about_dialog(this);
}


void MainWindow::start_watcher_for_location()
{
  watcher_.stop();
  if (location_.is_archive()) {
    // Watch the archive *file* (TOC / replacement) and the extract tree when
    // ready (member content under the cache).
    std::vector<std::filesystem::path> paths;
    paths.push_back(location_.as_path());
    if (const auto resolved = archive_manager_.resolved_directory(location_)) {
      paths.push_back(*resolved);
    }
    watcher_.set_location(fs::Location::from_archive(location_.as_path(), location_.entry_path()));
    watcher_.set_watch_paths(std::move(paths));
    watcher_.start();
    return;
  }
  watcher_.set_location(location_);
  watcher_.set_extra_paths({});
  watcher_.start();
}

void MainWindow::on_archive_ready(const fs::Location& archive_location,
                                  const std::filesystem::path& extracted_root)
{
  (void)extracted_root;
  QApplication::restoreOverrideCursor();
  // Refresh if we are still viewing this archive (or a path inside it).
  if (!location_.is_archive() || location_.as_path() != archive_location.as_path()) {
    return;
  }
  set_status(QStringLiteral("Archive ready — %1")
                             .arg(QString::fromStdString(archive_location.as_path().filename().string())));
  start_watcher_for_location();
  on_directory_changed();
}

void MainWindow::on_archive_failed(const fs::Location& archive_location, const QString& message)
{
  QApplication::restoreOverrideCursor();
  if (!location_.is_archive() || location_.as_path() != archive_location.as_path()) {
    return;
  }
  QMessageBox::warning(this, QStringLiteral("Archive"), message);
  set_status(message);
  // Fall back to parent directory so the user is not stuck on a failed archive view.
  open_location(fs::Location::from_path(archive_location.as_path().parent_path()), false);
}


void MainWindow::on_clear_filter()
{
  if (search_row_ != nullptr ? search_row_->isVisible()
      : (search_edit_ != nullptr && search_edit_->isVisible())) {
    stop_search();
    search_active_ = false;
    search_results_.clear();
    if (search_row_ != nullptr) {
      search_row_->hide();
    } else if (search_edit_ != nullptr) {
      search_edit_->hide();
    }
    search_edit_->clear();
    on_directory_changed();
    return;
  }
  if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()) {
    filter_edit_->clear();
    return;
  }
  if ((filter_row_ != nullptr ? filter_row_->isVisible()
         : (filter_edit_ != nullptr && filter_edit_->isVisible()))
      && !filter_pinned_
      && show_filter_act_ != nullptr && show_filter_act_->isChecked()) {
    show_filter_act_->setChecked(false);
    return;
  }
  if (location_edit_ != nullptr && location_edit_->isVisible()) {
    show_location_buttons();
  }
}

void MainWindow::on_view_middle_click(const QModelIndex& index)
{
  const fs::FileInfo* fi = model_->file_at(index.row());
  if (fi == nullptr) {
    return;
  }
  if (fi->is_directory()) {
    if (location_.is_archive()) {
      open_new_window(location_.join(fi->basename()));
    } else {
      open_new_window(fi->location());
    }
  } else if (fs::looks_like_archive(fi->path()) && !location_.is_archive()) {
    open_new_window(fs::Location::from_archive(fi->path(), {}));
  }
}


void MainWindow::on_show_leap()
{
  if (leap_widget_ != nullptr) {
    leap_widget_->show_and_focus();
  }
}

void MainWindow::jump_to_row(int row)
{
  if (model_ == nullptr || row < 0 || row >= model_->rowCount()) {
    return;
  }
  const QModelIndex idx = model_->index(row, 0);
  if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
    graphics_view_->select_row(row, true);
    // select_row materializes the item when needed; centre it in the viewport.
    const auto items = graphics_view_->scene()->selectedItems();
    if (!items.isEmpty()) {
      graphics_view_->ensureVisible(items.front(), 32, 32);
    } else if (row == 0) {
      graphics_view_->verticalScrollBar()->setValue(0);
    } else if (row >= model_->rowCount() - 1) {
      graphics_view_->verticalScrollBar()->setValue(
          graphics_view_->verticalScrollBar()->maximum());
    }
    return;
  }
  auto* view = current_view();
  if (view == nullptr || view->selectionModel() == nullptr) {
    return;
  }
  view->setCurrentIndex(idx);
  view->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  view->scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

void MainWindow::on_parent_new_window()
{
  open_new_window(location_.parent());
}

void MainWindow::on_rebuild_history_menu()
{
  if (history_menu_ == nullptr) {
    return;
  }
  history_menu_->clear();
  history_menu_->addAction(theme_icon("go-previous", "arrow-left"), QStringLiteral("Back"), this,
                           &MainWindow::on_go_back);
  history_menu_->addAction(theme_icon("go-next", "arrow-right"), QStringLiteral("Forward"), this,
                           &MainWindow::on_go_forward);
  history_menu_->addSeparator();

  // Folder / location history — most recent first.
  std::vector<fs::Location> locs;
  int count = 0;
  for (auto it = nav_history_.unique_locations().rbegin();
       it != nav_history_.unique_locations().rend() && count < 35; ++it, ++count) {
    locs.push_back(*it);
  }
  add_location_menu_entries(
      history_menu_, locs, this,
      [this](const fs::Location& loc) { open_location(loc); },
      [this](const fs::Location& loc) { open_new_window(loc); });
}

void MainWindow::on_rebuild_recent_opens_menu()
{
  if (recent_opens_menu_ == nullptr) {
    return;
  }
  populate_recent_opens_menu(recent_opens_menu_, 30);
  recent_opens_menu_->addSeparator();
  recent_opens_menu_->addAction(theme_icon("view-history", "document-open-recent"),
                                QStringLiteral("Open History…"), this, [this] {
                                  show_open_history_dialog(this, [this](const QString& dir) {
                                    open_location(fs::Location::from_path(dir.toStdString()));
                                  });
                                });
}


void MainWindow::on_toggle_bookmark()
{
  if (location_.empty()) {
    return;
  }
  const bool now = bookmarks_.toggle(location_);
  if (message_area_ != nullptr) {
    if (now) {
      message_area_->show_info(QStringLiteral("Bookmark added"));
    } else {
      message_area_->show_info(QStringLiteral("Bookmark removed"));
    }
  }
  set_status(now ? QStringLiteral("Bookmarked") : QStringLiteral("Bookmark removed"));
  rebuild_sidebar_places();
}

void MainWindow::on_rebuild_bookmarks_menu()
{
  if (bookmarks_menu_ == nullptr) {
    return;
  }
  bookmarks_menu_->clear();

  if (location_.empty()) {
    auto* act = bookmarks_menu_->addAction(QStringLiteral("No location to bookmark"));
    act->setEnabled(false);
  } else if (bookmarks_.contains(location_)) {
    bookmarks_menu_->addAction(theme_icon("bookmark-remove", "list-remove"),
                               QStringLiteral("Remove Bookmark for This Location"), this,
                               &MainWindow::on_toggle_bookmark);
  } else {
    bookmarks_menu_->addAction(theme_icon("bookmark-new", "list-add"),
                               QStringLiteral("Bookmark This Location"), this,
                               &MainWindow::on_toggle_bookmark);
  }
  bookmarks_menu_->addSeparator();

  const auto entries = bookmarks_.entries();
  if (entries.empty()) {
    auto* act = bookmarks_menu_->addAction(QStringLiteral("(no bookmarks yet)"));
    act->setEnabled(false);
    return;
  }

  add_location_menu_entries(
      bookmarks_menu_, entries, this,
      [this](const fs::Location& loc) { open_location(loc); },
      [this](const fs::Location& loc) { open_new_window(loc); });
}


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


void MainWindow::apply_detail_column_visibility()
{
  if (tree_view_ == nullptr || model_ == nullptr) {
    return;
  }
  // Name (0) always visible.
  auto visible = [this](const char* key) {
    if (detail_columns_.isEmpty()) {
      // Defaults: everything except optional Width/Height.
      return QLatin1String(key) != QLatin1String("width")
          && QLatin1String(key) != QLatin1String("height");
    }
    return detail_columns_.contains(QLatin1String(key));
  };
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Size), !visible("size"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Width), !visible("width"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Height), !visible("height"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Dimensions), !visible("dimensions"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::AspectRatio), !visible("aspectratio"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Framerate), !visible("framerate"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Duration), !visible("duration"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Modified), !visible("modified"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Type), !visible("type"));
}


} // namespace dirtoo::app

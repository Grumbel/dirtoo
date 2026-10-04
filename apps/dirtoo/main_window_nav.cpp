// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include "async_io.hpp"
#include "path_availability.hpp"
#include "mime_util.hpp"
#include "activity_monitor.hpp"
#include "location_menu_helpers.hpp"
#include "location_icons.hpp"
#include <QMenu>
#include <QAbstractScrollArea>
#include <QStackedWidget>
#include <QScrollBar>
#include <algorithm>
#include <QCursor>

#include "archive_listing.hpp"
#include "dirtoo/filter/media_meta_cache.hpp"
#include <QDir>
#include <QTimer>
#include <QtConcurrent>
#include <QFileInfo>
#include <QSet>
#include <QMimeDatabase>
#include <QThreadPool>
#include <QStandardPaths>
#include <filesystem>
#include <set>

namespace dirtoo::app {

void MainWindow::open_location(const fs::Location& location, bool record_history)
{

  qInfo().noquote() << QStringLiteral("open_location %1 (history=%2)")
                           .arg(QString::fromStdString(location.as_url()))
                           .arg(record_history);
  stop_search();
  search_session_.active = false;
  search_session_.results.clear();
  filter_search_.set_search_visible(false);

  const bool has_loc_filter = !location.filter_query().empty();
  const bool has_loc_search = !location.search_query().empty();

  // Reset filter on directory change unless Pin Filter is active or the
  // incoming location carries ?filter=.
  if (!has_loc_filter && !filter_pinned_ && !filter_search_.filter_text().isEmpty()) {
    if (auto* edit = filter_search_.filter_edit()) {
      edit->blockSignals(true);
      edit->clear();
      edit->blockSignals(false);
    }
    collection_.set_name_filter(std::string{});
    update_filter_chrome(false);
  }
  location_ = location;
  location_chrome_.set_location(location_);
  location_chrome_.set_bookmarked(!location_.empty() && bookmarks_.contains(location_));
  update_window_title();

  if (record_history) {
    nav_history_.push(location, true, capture_view_scroll());
    pending_nav_scroll_.reset();
  }
  update_history_actions();

  // Apply ?filter= / ?search= after chrome is synced (search starts after load
  // for file locations; filter can apply immediately).
  if (has_loc_filter || has_loc_search) {
    apply_location_queries(location);
  }

  if (location_.is_tag()) {
    // Virtual tag collection — no directory watcher, no disk list worker.
    stop_search();
    search_session_.active = false;
    watcher_.stop();
    // Async load: do not leave the previous folder's entries actionable meanwhile.
    collection_.clear();
    refresh_list();
    load_tag_location_listing();
    if (auto* view = current_view()) {
      view->setFocus(Qt::OtherFocusReason);
    }
    return;
  }

  if (location_.is_set()) {
    stop_search();
    search_session_.active = false;
    watcher_.stop();
    // Async load: do not leave the previous folder's entries actionable meanwhile.
    collection_.clear();
    refresh_list();
    load_set_location_listing();
    if (auto* view = current_view()) {
      view->setFocus(Qt::OtherFocusReason);
    }
    return;
  }

  if (location_.is_archive()) {
    dir_session_.pending_archive_location = location_;

    // Reuse index when navigating within the same archive file.
    if (archive_listing_.ready_for(location_.as_path())) {
      start_watcher_for_location();
      on_directory_changed();
    } else {
      // TOC listing can take a long time on large archives — never block the GUI.
      const auto path = location_.as_path();
      const quint64 gen = ++archive_index_generation_;
      set_status(QStringLiteral("Indexing archive…"));
      ActivityMonitor::instance().set_task(QStringLiteral("archive-index"),
                                           QStringLiteral("Indexing archive…"), -1, -1);
      update_busy_indicator(QStringLiteral("Indexing archive…"));
      struct Indexed {
        ArchiveListing listing;
        std::string error;
        bool ok = false;
      };
      run_io(
          this,
          [path] {
            Indexed r;
            r.ok = r.listing.load(path, &r.error);
            return r;
          },
          [this, path, gen](Indexed indexed) {
              const bool ok = indexed.ok;
              const std::string& list_err = indexed.error;
              auto& listing = indexed.listing;
              if (gen != archive_index_generation_ || location_.as_path() != path) {
                return; // navigated away
              }
              ActivityMonitor::instance().clear_task(QStringLiteral("archive-index"));
              update_busy_indicator({});
              if (ok) {
                archive_listing_ = std::move(listing);
                set_status(QStringLiteral("Archive index: %1 entries")
                               .arg(archive_listing_.entries().size()));
                start_watcher_for_location();
                on_directory_changed();
              } else {
                set_status(QStringLiteral("Listing failed (%1); extracting…")
                               .arg(QString::fromStdString(list_err)));
                if (archive_manager_.status(fs::Location::from_archive(path, {}))
                    != archive::ExtractStatus::Ready) {
                  QApplication::setOverrideCursor(Qt::WaitCursor);
                }
                archive_manager_.open(location_);
              }
          });
    }
  } else {
    // List first so status/activity can show “Loading…” before anything that may
    // block on a slow volume (inotify_add_watch, sidebar path walk).
    reload_directory(false);
    QTimer::singleShot(0, this, [this] {
      if (search_session_.active || location_.is_archive() || location_.is_tag() || location_.is_set()) {
        return;
      }
      start_watcher_for_location();
      sync_sidebar_to_location();
    });
  }

  if (auto* view = current_view()) {
    view->setFocus(Qt::OtherFocusReason);
  }
  // Archive path still syncs sidebar immediately (index is local once loaded).
  if (location_.is_archive()) {
    sync_sidebar_to_location();
  }

}

void MainWindow::on_location_entered(const QString& text)
{
  try {
    open_location(fs::Location::from_human(text.toStdString()));
  } catch (const std::exception& ex) {
    set_status(QString::fromUtf8(ex.what()));
  }
}

void MainWindow::on_go_parent()
{
  // Location bar treats ?filter= / ?search= as the tip segment. Parent should
  // strip that tip first (stay on the same path) before walking the filesystem.
  if (!location_.filter_query().empty() || !location_.search_query().empty()) {
    location_chrome_.clear_query_tip();
    open_location(location_.with_filter_and_search({}, {}));
    return;
  }
  open_location(location_.parent());
}

void MainWindow::on_go_home()
{
  open_location(fs::Location::from_path(std::filesystem::path{QDir::homePath().toStdString()}));
}

void MainWindow::on_go_back()
{
  nav_history_.update_current_scroll(capture_view_scroll());
  if (const auto entry = nav_history_.go_back()) {
    pending_nav_scroll_ = entry->scroll_y;
    open_location(entry->location, false);
  }
}

void MainWindow::on_go_forward()
{
  nav_history_.update_current_scroll(capture_view_scroll());
  if (const auto entry = nav_history_.go_forward()) {
    pending_nav_scroll_ = entry->scroll_y;
    open_location(entry->location, false);
  }
}

void MainWindow::on_back_history_menu(const QPoint& pos)
{
  const auto& stack = nav_history_.stack();
  const int cur = nav_history_.index();
  if (cur <= 0 || stack.empty()) {
    return;
  }
  QMenu menu(this);
  for (int i = cur - 1; i >= 0; --i) {
    const fs::Location& loc = stack[static_cast<std::size_t>(i)].location;
    auto* act = menu.addAction(icon_for_location(loc), location_menu_label(loc));
    PathAvailability::instance().track(act, loc);
    const int idx = i;
    connect(act, &QAction::triggered, this, [this, idx] {
      nav_history_.update_current_scroll(capture_view_scroll());
      if (const auto jumped = nav_history_.go_to_index(idx)) {
        pending_nav_scroll_ = jumped->scroll_y;
        open_location(jumped->location, false);
      }
    });
  }
  if (menu.isEmpty()) {
    return;
  }
  auto* w = qobject_cast<QWidget*>(sender());
  menu.exec(w != nullptr ? w->mapToGlobal(pos) : QCursor::pos());
}

void MainWindow::on_forward_history_menu(const QPoint& pos)
{
  const auto& stack = nav_history_.stack();
  const int cur = nav_history_.index();
  if (cur < 0 || cur + 1 >= static_cast<int>(stack.size())) {
    return;
  }
  QMenu menu(this);
  for (int i = cur + 1; i < static_cast<int>(stack.size()); ++i) {
    const fs::Location& loc = stack[static_cast<std::size_t>(i)].location;
    auto* act = menu.addAction(icon_for_location(loc), location_menu_label(loc));
    PathAvailability::instance().track(act, loc);
    const int idx = i;
    connect(act, &QAction::triggered, this, [this, idx] {
      nav_history_.update_current_scroll(capture_view_scroll());
      if (const auto jumped = nav_history_.go_to_index(idx)) {
        pending_nav_scroll_ = jumped->scroll_y;
        open_location(jumped->location, false);
      }
    });
  }
  if (menu.isEmpty()) {
    return;
  }
  auto* w = qobject_cast<QWidget*>(sender());
  menu.exec(w != nullptr ? w->mapToGlobal(pos) : QCursor::pos());
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


void MainWindow::on_directory_changed()
{
  reload_directory(false);
  // Soft-refresh expanded sidebar tree node for the current folder (if any).
  if (auto* tree_model = sidebar_.places().model()) {
    if (!location_.is_archive()) {
      tree_model->refresh_if_loaded(QString::fromStdString(location_.as_path().string()));
    }
  }
}

void MainWindow::on_entries_changed(const QStringList& created, const QStringList& removed,
                                    const QStringList& modified)
{
  // Prefer incremental updates when the change set is small and we are not in a
  // mode that needs a full rescan (search, archive index, content filter).
  if (search_session_.active || location_.is_archive()) {
    if (watcher_reload_timer_ != nullptr) {
      watcher_reload_timer_->start();
    }
    return;
  }
  const int n = created.size() + removed.size() + modified.size();
  if (n == 0) {
    return;
  }
  // Large bursts (e.g. unpack) — fall back to soft full merge.
  if (n > 48) {
    if (watcher_reload_timer_ != nullptr) {
      watcher_reload_timer_->start();
    }
    return;
  }
  if (!filter_search_.filter_text().isEmpty()) {
    // Content/name filter: cheaper to soft-reload and re-filter than patch visible.
    if (watcher_reload_timer_ != nullptr) {
      watcher_reload_timer_->start();
    }
    return;
  }

  // Sidebar tree: soft-refresh parent dirs of created/removed subdirs when loaded.
  if (auto* tree_model = sidebar_.places().model()) {
    QSet<QString> parents;
    auto add_parent = [&](const QString& path) {
      const QString parent = QFileInfo(path).absolutePath();
      if (!parent.isEmpty()) {
        parents.insert(parent);
      }
    };
    for (const QString& path : created) {
      add_parent(path);
    }
    for (const QString& path : removed) {
      add_parent(path);
    }
    for (const QString& parent : parents) {
      tree_model->refresh_if_loaded(parent);
    }
  }

  // Removals need no FS I/O — path identity only.
  bool removed_any = false;
  for (const QString& path : removed) {
    const auto loc = fs::Location::from_path(std::filesystem::path{path.toStdString()});
    if (collection_.remove(loc)) {
      removed_any = true;
    }
    if (model_ != nullptr) {
      model_->clear_thumbnail(path);
    }
  }
  if (removed_any && model_ != nullptr) {
    model_->refresh();
    update_status_selection();
  }

  // Create/modify: exists() + FileInfo::from_path (stat) can block for seconds on a
  // spinning USB volume. Never do that on the GUI thread.
  QStringList upsert_paths = created;
  for (const QString& path : modified) {
    if (!upsert_paths.contains(path)) {
      upsert_paths.append(path);
    }
  }
  if (upsert_paths.isEmpty()) {
    if (removed_any) {
      schedule_directory_thumbnails_low_priority();
    }
    return;
  }

  const QStringList created_copy = created;
  run_io(
      this,
      [upsert_paths] {
        std::vector<fs::FileInfo> infos;
        infos.reserve(static_cast<std::size_t>(upsert_paths.size()));
        for (const QString& path : upsert_paths) {
          std::error_code ec;
          const std::filesystem::path p{path.toStdString()};
          if (!std::filesystem::exists(std::filesystem::symlink_status(p, ec)) || ec) {
            continue;
          }
          infos.push_back(fs::FileInfo::from_path(p));
        }
        return infos;
      },
      [this, created_copy](std::vector<fs::FileInfo> infos) {
        apply_watcher_upserts(std::move(infos), created_copy);
      });
}

void MainWindow::apply_watcher_upserts(std::vector<fs::FileInfo> infos,
                                       const QStringList& created_paths)
{
  if (search_session_.active) {
    return;
  }
  bool changed = false;
  for (auto& info : infos) {
    if (const auto idx = collection_.index_of(info.location())) {
      (void)idx;
      collection_.remove(info.location());
      collection_.add(std::move(info));
    } else {
      collection_.add(std::move(info));
    }
    changed = true;
  }
  if (!changed) {
    return;
  }
  if (model_ != nullptr) {
    model_->refresh();
  }
  update_status_selection();

  // Thumbnails for newly created regular files (extension MIME only — no stat).
  if (model_ != nullptr && !created_paths.isEmpty()) {
    std::vector<fs::Location> locs;
    QStringList mimes;
    for (const QString& path : created_paths) {
      const auto loc = fs::Location::from_path(std::filesystem::path{path.toStdString()});
      const auto idx = collection_.index_of(loc);
      if (!idx) {
        continue;
      }
      const auto& existing = collection_.items()[*idx];
      if (!existing.is_regular_file()) {
        if (existing.is_directory()) {
          model_->clear_thumbnail(path); // stale montage
        }
        continue;
      }
      model_->clear_thumbnail(path);
      model_->set_thumbnail_pending(path);
      locs.push_back(loc);
      mimes.push_back(mime_for_thumbnail_fast(path));
    }
    if (!locs.empty()) {
      request_thumbnails_for_paths(locs, mimes);
    }
  }
  request_thumbnails_for_visible();
  schedule_directory_thumbnails_low_priority();
}



int MainWindow::capture_view_scroll() const
{
  QWidget* w = nullptr;
  if (view_stack_ != nullptr) {
    w = view_stack_->currentWidget();
  }
  if (w == nullptr) {
    return 0;
  }
  // QAbstractItemView / QGraphicsView both expose verticalScrollBar().
  if (auto* scroll_area = qobject_cast<QAbstractScrollArea*>(w)) {
    if (QScrollBar* sb = scroll_area->verticalScrollBar()) {
      return sb->value();
    }
  }
  return 0;
}

void MainWindow::restore_view_scroll(int scroll_y)
{
  QWidget* w = nullptr;
  if (view_stack_ != nullptr) {
    w = view_stack_->currentWidget();
  }
  if (w == nullptr) {
    return;
  }
  if (auto* scroll_area = qobject_cast<QAbstractScrollArea*>(w)) {
    if (QScrollBar* sb = scroll_area->verticalScrollBar()) {
      const int y = std::clamp(scroll_y, sb->minimum(), sb->maximum());
      sb->setValue(y);
    }
  }
}

void MainWindow::apply_pending_nav_scroll()
{
  if (!pending_nav_scroll_.has_value()) {
    return;
  }
  const int y = *pending_nav_scroll_;
  // Defer until after layout/relayout so scrollbar maximum is correct.
  // Keep pending until a later pass (sort/filter) can re-apply when the
  // range grows; cleared when a new history entry is recorded.
  QTimer::singleShot(0, this, [this, y] {
    restore_view_scroll(y);
  });
}

} // namespace dirtoo::app

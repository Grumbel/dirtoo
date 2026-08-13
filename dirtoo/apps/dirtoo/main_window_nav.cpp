// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"
#include "location_menu_helpers.hpp"
#include "location_icons.hpp"
#include <QMenu>
#include <QCursor>

#include "archive_listing.hpp"
#include "dirtoo/filter/media_meta_cache.hpp"
#include <QDir>
#include <QTimer>
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
  // Reset filter on directory change unless Pin Filter is active.
  if (!filter_pinned_ && !filter_search_.filter_text().isEmpty()) {
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
  update_window_title();

  nav_history_.push(location, record_history);
  update_history_actions();

  if (location_.is_archive()) {
    dir_session_.pending_archive_location = location_;

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
    // List first so status/activity can show “Loading…” before anything that may
    // block on a slow volume (inotify_add_watch, sidebar path walk).
    reload_directory(false);
    QTimer::singleShot(0, this, [this] {
      if (search_session_.active || location_.is_archive()) {
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

void MainWindow::on_back_history_menu(const QPoint& pos)
{
  const auto& stack = nav_history_.stack();
  const int cur = nav_history_.index();
  if (cur <= 0 || stack.empty()) {
    return;
  }
  QMenu menu(this);
  for (int i = cur - 1; i >= 0; --i) {
    const fs::Location& loc = stack[static_cast<std::size_t>(i)];
    auto* act = menu.addAction(icon_for_location(loc), location_menu_label(loc));
    const int idx = i;
    connect(act, &QAction::triggered, this, [this, idx] {
      if (const auto jumped = nav_history_.go_to_index(idx)) {
        open_location(*jumped, false);
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
    const fs::Location& loc = stack[static_cast<std::size_t>(i)];
    auto* act = menu.addAction(icon_for_location(loc), location_menu_label(loc));
    const int idx = i;
    connect(act, &QAction::triggered, this, [this, idx] {
      if (const auto jumped = nav_history_.go_to_index(idx)) {
        open_location(*jumped, false);
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
  QThreadPool::globalInstance()->start([this, upsert_paths, created_copy] {
    std::vector<fs::FileInfo> infos;
    infos.reserve(static_cast<std::size_t>(upsert_paths.size()));
    for (const QString& path : upsert_paths) {
      std::error_code ec;
      const std::filesystem::path p{path.toStdString()};
      if (!std::filesystem::exists(p, ec) || ec) {
        continue;
      }
      infos.push_back(fs::FileInfo::from_path(p));
    }
    QMetaObject::invokeMethod(
        this, "apply_watcher_upserts", Qt::QueuedConnection,
        Q_ARG(std::vector<dirtoo::fs::FileInfo>, infos),
        Q_ARG(QStringList, created_copy));
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
    QMimeDatabase mime_db;
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
      mimes.push_back(mime_db.mimeTypeForFile(path, QMimeDatabase::MatchExtension).name());
    }
    if (!locs.empty()) {
      request_thumbnails_for_paths(locs, mimes);
    }
  }
  request_thumbnails_for_visible();
  schedule_directory_thumbnails_low_priority();
}


} // namespace dirtoo::app

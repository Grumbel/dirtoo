// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include "archive_listing.hpp"
#include "dirtoo/filter/media_meta_cache.hpp"
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
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


void MainWindow::on_directory_changed()
{
  reload_directory(false);
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
  if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()) {
    // Content/name filter: cheaper to soft-reload and re-filter than patch visible.
    if (watcher_reload_timer_ != nullptr) {
      watcher_reload_timer_->start();
    }
    return;
  }

  bool changed = false;
  for (const QString& path : removed) {
    const auto loc = fs::Location::from_path(std::filesystem::path{path.toStdString()});
    if (collection_.remove(loc)) {
      changed = true;
    }
  }
  auto upsert = [this, &changed](const QString& path) {
    std::error_code ec;
    const std::filesystem::path p{path.toStdString()};
    if (!std::filesystem::exists(p, ec) || ec) {
      return;
    }
    auto info = fs::FileInfo::from_path(p);
    if (const auto idx = collection_.index_of(info.location())) {
      // Replace via merge of a single-item list would drop others; use remove+add.
      collection_.remove(info.location());
      collection_.add(std::move(info));
    } else {
      collection_.add(std::move(info));
    }
    changed = true;
  };
  for (const QString& path : created) {
    upsert(path);
  }
  for (const QString& path : modified) {
    upsert(path);
  }
  if (!changed) {
    return;
  }
  // Keep sort stable for small patches; optional async sort if user prefers name order
  // of brand-new files at the end until next explicit sort.
  if (model_ != nullptr) {
    model_->refresh();
  }
  update_status_selection();
  // New files often sit outside the current viewport batch; request them by path
  // so "Reload Thumbnails" is not required for freshly created items.
  if (model_ != nullptr && !created.isEmpty()) {
    QMimeDatabase mime_db;
    std::vector<fs::Location> locs;
    QStringList mimes;
    for (const QString& path : created) {
      std::error_code ec;
      const std::filesystem::path p{path.toStdString()};
      if (!std::filesystem::is_regular_file(p, ec) || ec) {
        continue;
      }
      model_->clear_thumbnail(path);
      model_->set_thumbnail_pending(path);
      locs.push_back(fs::Location::from_path(p));
      mimes.push_back(mime_db.mimeTypeForFile(path, QMimeDatabase::MatchExtension).name());
    }
    if (!locs.empty()) {
      request_thumbnails_for_paths(locs, mimes);
    }
  }
  request_thumbnails_for_visible();
  // Directory content changed: drop stale montage status so low-priority regen can run.
  if (model_ != nullptr) {
    for (const QString& path : created + removed + modified) {
      std::error_code ec;
      const std::filesystem::path p{path.toStdString()};
      if (std::filesystem::is_directory(p, ec) && !ec) {
        model_->clear_thumbnail(path);
      }
    }
  }
  schedule_directory_thumbnails_low_priority();
}


} // namespace dirtoo::app

#include "main_window.hpp"
#include "archive_listing.hpp"
#include "directory_tree_model.hpp"
#include "theme_icons.hpp"

#include "dirtoo/filter/media_meta_cache.hpp"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QTimer>
#include <QTreeView>
#include <QSplitter>
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


void MainWindow::on_directory_changed()
{
  reload_directory(false);
}

void MainWindow::on_entries_changed(const QStringList& created, const QStringList& removed,
                                    const QStringList& modified)
{
  // Prefer incremental updates when the change set is small and we are not in a
  // mode that needs a full rescan (search, archive index, content filter).
  if (search_active_ || location_.is_archive()) {
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
      thumbnailer_.request_many(locs, mimes, QStringLiteral("large"));
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


} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include "archive_listing.hpp"
#include "tag_paint.hpp"
#include <QSet>
#include <QStringList>
#include <filesystem>

namespace dirtoo::app {

void MainWindow::reload_directory(bool soft)
{
  if (search_session_.active) {
    // Keep recursive search results until the user navigates away or closes search.
    return;
  }
  qInfo().noquote() << QStringLiteral("reload_directory soft=%1 path=%2")
                           .arg(soft)
                           .arg(QString::fromStdString(location_.as_url()));
  dir_session_.soft_reload = soft;
  if (!soft) {
    // Navigation/explicit refresh supersedes any pending soft watcher tick.
    if (watcher_reload_timer_ != nullptr) {
      watcher_reload_timer_->stop();
    }
    cancel_all_thumbnails();
    clear_thumb_aliases();
    if (model_ != nullptr) {
      model_->clear_thumbnails();
    }
    // External dt-tag / other processes may have changed tags; drop paint cache.
    tag_paint_detail::clear_tag_chip_cache();
    filter::MediaMetaCache::instance().bump_generation();
  }

  // In-memory archive index: apply on UI thread (no directory walk).
  // Soft watcher ticks refresh the TOC only when the archive file stamp changes.
  if (location_.is_archive()) {
    std::string list_err;
    (void)archive_listing_.refresh_if_stale(location_.as_path(), &list_err);
  }
  if (location_.is_archive() && archive_listing_.ok()) {
    dir_session_.soft_reload = false;
    auto items = archive_listing_.fileinfos_for(location_);
    if (model_ != nullptr) {
      model_->clear_child_counts();
      for (const auto& [path, n] : archive_listing_.child_counts_for(location_)) {
        model_->set_child_count(QString::fromStdString(path), static_cast<qint64>(n));
      }
    }
    collection_.sorter().set_ascending(sort_ascending_);
    collection_.set_items(std::move(items));
    if (!filter_search_.filter_text().isEmpty()) {
      if (filter_expression_needs_content_io(filter_search_.filter_text())) {
        request_async_filter();
      } else {
        collection_.set_name_filter(filter_search_.filter_text().toStdString());
      }
    }
    refresh_list();
    request_thumbnails_for_visible();
  rebuild_quick_filters();
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

  const quint64 gen = list_workers_.next_dir_load_generation();
  set_status(soft ? QStringLiteral("Refreshing…") : QStringLiteral("Loading…"));
  // Hard navigation: clear immediately so the old directory does not linger.
  // Soft (watcher): keep showing the previous listing until the worker finishes.
  if (!soft) {
    collection_.clear();
    refresh_list();
  }

  if (list_workers_.dir_load() == nullptr) {
    return;
  }
  // Supersede any in-flight listing (especially important when soft refreshes stack up).
  QMetaObject::invokeMethod(list_workers_.dir_load(), "cancel", Qt::QueuedConnection);
  const QString path = QString::fromStdString(load_loc.as_path().string());
  QMetaObject::invokeMethod(list_workers_.dir_load(), "load", Qt::QueuedConnection,
                            Q_ARG(QString, path), Q_ARG(quint64, gen));
}

void MainWindow::on_directory_loaded(quint64 generation, std::vector<fs::FileInfo> items)
{
  qInfo().noquote() << QStringLiteral("directory_loaded gen=%1 items=%2")
                           .arg(generation)
                           .arg(items.size());

  if (generation != list_workers_.dir_load_generation() || search_session_.active) {
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
    if (dir_session_.known_paths_location != location_) {
      // Different folder (or first load): no "new" stickers for the initial set.
      model_->clear_new_marks();
    } else if (!dir_session_.known_paths.empty()) {
      for (const QString& p : next_paths) {
        if (!dir_session_.known_paths.contains(p)) {
          model_->mark_new(p);
        }
      }
      // Drop thumbs for paths that vanished; drop "new" marks for those too.
      for (const QString& p : dir_session_.known_paths) {
        if (!next_paths.contains(p)) {
          model_->clear_thumbnail(p);
        }
      }
      model_->prune_new_marks(next_paths);
    }
    dir_session_.known_paths = next_paths;
    dir_session_.known_paths_location = location_;
  }
  collection_.sorter().set_ascending(sort_ascending_);
  const bool soft = dir_session_.soft_reload;
  dir_session_.soft_reload = false;
  // Soft watcher refresh: merge into existing collection (preserve order of
  // survivors, append newcomers) instead of replacing the whole vector.
  const bool content_filter =
      !filter_search_.filter_text().isEmpty()
      && filter_expression_needs_content_io(filter_search_.filter_text());
  if (soft && !collection_.empty()) {
    // Avoid rebuild_visible when content matchers would hit the GUI thread.
    collection_.merge_items(std::move(items), !content_filter);
  } else {
    collection_.set_items_unsorted(std::move(items));
  }
  if (!filter_search_.filter_text().isEmpty()) {
    if (content_filter) {
      // Keep previous visible list until FilterWorker finishes (no empty flash).
      request_async_filter(/*keep_previous_visible=*/true);
    } else {
      collection_.set_name_filter(filter_search_.filter_text().toStdString());
    }
  }
  // Soft (watcher) reloads: keep the previous paint until sort finishes to avoid
  // unsorted→sorted double flicker. Hard navigation still paints ASAP.
  if (!soft) {
    refresh_list();
  }
  set_status(QStringLiteral("%1 items").arg(
      soft ? dir_session_.known_paths.size() : collection_.visible_items().size()));
  // After soft path, visible may still be old until sort/filter apply; status updated again later.
  // Content filters own the visible list via FilterWorker; replace_items_sorted would
  // rebuild_visible with matchers (GUI I/O) or wipe the async filter result.
  if (!content_filter) {
    request_async_sort();
  }
  request_thumbnails_for_visible();
  rebuild_quick_filters();
}

void MainWindow::on_directory_load_failed(quint64 generation, QString error)
{
  qWarning().noquote() << QStringLiteral("directory_load_failed gen=%1: %2")
                              .arg(generation)
                              .arg(error);

  if (generation != list_workers_.dir_load_generation()) {
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

void MainWindow::rebuild_quick_filters()
{
  if (quick_filter_bar_ == nullptr) {
    return;
  }
  quick_filter_bar_->rebuild_from_items(collection_.items());
  quick_filter_bar_->set_active_expression(filter_search_.filter_text());
}

} // namespace dirtoo::app

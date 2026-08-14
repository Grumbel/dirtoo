// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"
#include "mime_util.hpp"
#include "directory_thumbnail_worker.hpp"

#include "dirtoo/archive/archive_index.hpp"
#include "dirtoo/thumbnail/thumbnailer.hpp"
#include "dirtoo/filter/media_meta_cache.hpp"
#include <QDebug>
#include <QSet>
#include <algorithm>
#include <functional>
#include <memory>
#include <QFile>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <QGraphicsItem>
#include <filesystem>
#include <optional>
#include <QIcon>
#include <QPixmap>

namespace dirtoo::app {

void MainWindow::cancel_all_thumbnails()
{
  thumbs_.cancel_all();
  if (model_ != nullptr) {
    model_->clear_pending_thumbnails();
  }
}

void MainWindow::clear_thumb_aliases()
{
  thumbs_.clear_aliases();
}

void MainWindow::request_thumbnails_for_paths(const std::vector<fs::Location>& locs,
                                             const QStringList& mimes)
{
  thumbs_.request_many(locs, mimes);
}

void MainWindow::wire_thumbnail_services()
{
  thumbs_.wire_ready_failed(this, &MainWindow::on_thumbnail_ready,
                            &MainWindow::on_thumbnail_failed);
}

void MainWindow::shutdown_thumbnail_workers()
{
  thumbs_.shutdown();
}



void MainWindow::schedule_directory_thumbnails_low_priority()
{
  // Defer expensive montages until after ordinary file thumbs have had a chance.
  QTimer::singleShot(2500, this, [this] {
    if (thumbs_.dir_worker() == nullptr || model_ == nullptr) {
      return;
    }
    QStringList dirs;
    for (const auto& fi : collection_.visible_items()) {
      // Search hits are synthetic but still refer to real local directories —
      // only skip empty / non-directory entries.
      if (!fi.is_directory() || fi.path().empty()) {
        continue;
      }
      const QString path = QString::fromStdString(fi.path().string());
      const QString cache =
          thumbnail::Thumbnailer::cache_path_for(fi.location(), QStringLiteral("large"));
      if (QFile::exists(cache)) {
        // Keep showing existing montage; skip rebuild unless cleared.
        if (model_->thumbnail_status(path) != ThumbnailStatus::Ready) {
          model_->set_thumbnail(path, QIcon(cache));
        }
        continue;
      }
      if (model_->thumbnail_status(path) == ThumbnailStatus::Pending
          || model_->thumbnail_status(path) == ThumbnailStatus::Ready) {
        continue;
      }
      dirs << path;
      model_->set_thumbnail_pending(path);
    }
    if (dirs.isEmpty()) {
      return;
    }
    // Cap batch size so a huge folder listing does not monopolize the worker.
    constexpr int kMax = 12;
    if (dirs.size() > kMax) {
      dirs = dirs.mid(0, kMax);
    }
    QMetaObject::invokeMethod(thumbs_.dir_worker(), "generate", Qt::QueuedConnection,
                              Q_ARG(QStringList, dirs));
  });
}

void MainWindow::request_thumbnails_for_visible()
{
  // Debounce rapid refresh/scroll storms (generation-safe via singleShot capturing this).
  // Slightly longer than a single wheel tick so flings coalesce into one request.
  QTimer::singleShot(120, this, [this] {
    const auto& visible = collection_.visible_items();
    if (visible.empty()) {
      return;
    }
    // Lost D-Bus Ready/Error leaves Pending forever; re-queue after 60s idle.
    if (model_ != nullptr) {
      model_->clear_stale_pending_thumbnails(60000);
    }

    // Prefer true viewport row ranges from layout / view geometry. Do not use
    // scene()->items(): that only reports *materialized* sparse tiles and
    // diverges from on-screen slots after resize. Truncating to the lowest
    // indices then dropped the newly exposed high-index rows.
    std::vector<int> rows;
    bool from_viewport = false;
    if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
      rows = graphics_view_->viewport_model_rows();
      from_viewport = !rows.empty();
    } else if (QAbstractItemView* view = current_view()) {
      const QRect vp = view->viewport()->rect();
      // List mode is multi-column (top-to-bottom, then left-to-right). Sampling
      // only x=4 missed every column after the first when the window is wide.
      // Detail is single-column; still sample a few x positions for safety.
      QSet<int> row_set;
      int step_y = 24;
      int step_x = vp.width(); // one column by default (Detail)
      if (view_mode_ == ViewMode::List && icon_view_ != nullptr) {
        const QSize grid = icon_view_->gridSize();
        step_x = std::max(48, grid.width() > 0 ? grid.width() : 180);
        step_y = std::max(16, grid.height() > 0 ? grid.height() : 24);
      } else {
        const int hint = view->sizeHintForRow(0);
        step_y = std::max(8, hint > 0 ? hint : 24);
        step_x = std::max(64, vp.width() / 4);
      }
      for (int x = 4; x < vp.width() + step_x; x += step_x) {
        const int sx = std::min(x, std::max(0, vp.width() - 4));
        for (int y = 4; y < vp.height() + step_y; y += step_y) {
          const int sy = std::min(y, std::max(0, vp.height() - 4));
          const QModelIndex idx = view->indexAt(QPoint(sx, sy));
          if (idx.isValid()) {
            row_set.insert(idx.row());
          }
        }
      }
      if (row_set.isEmpty()) {
        const QModelIndex top = view->indexAt(vp.topLeft() + QPoint(4, 4));
        const QModelIndex bot = view->indexAt(vp.bottomRight() - QPoint(4, 4));
        if (top.isValid()) {
          row_set.insert(top.row());
        }
        if (bot.isValid()) {
          row_set.insert(bot.row());
        }
      }
      if (!row_set.isEmpty()) {
        int first = *std::min_element(row_set.begin(), row_set.end());
        int last = *std::max_element(row_set.begin(), row_set.end());
        first = std::max(0, first - 4);
        last = std::min(model_->rowCount() - 1, last + 8);
        // List mode row indices are not contiguous on screen (columns interleave
        // in model order). Prefer the actual set plus a small pad of neighbours.
        if (view_mode_ == ViewMode::List) {
          QSet<int> expanded = row_set;
          for (int r : row_set) {
            for (int d = -2; d <= 2; ++d) {
              const int rr = r + d;
              if (rr >= 0 && rr < model_->rowCount()) {
                expanded.insert(rr);
              }
            }
          }
          rows.assign(expanded.begin(), expanded.end());
          std::sort(rows.begin(), rows.end());
        } else {
          for (int r = first; r <= last; ++r) {
            rows.push_back(r);
          }
        }
        from_viewport = !rows.empty();
      }
    }

    // Cap viewport batches tightly: large scroll flings used to queue hundreds of
    // Thumbnailer1 jobs and leave Pending marks for rows that scrolled away.
    constexpr int kMaxFallback = 48;
    constexpr int kMaxViewport = 64;
    if (rows.empty()) {
      const int n = std::min(static_cast<int>(visible.size()), kMaxFallback);
      rows.reserve(static_cast<std::size_t>(n));
      for (int r = 0; r < n; ++r) {
        rows.push_back(r);
      }
    } else if (!from_viewport && static_cast<int>(rows.size()) > kMaxFallback) {
      rows.resize(static_cast<std::size_t>(kMaxFallback));
    } else if (from_viewport && static_cast<int>(rows.size()) > kMaxViewport) {
      const int mid = static_cast<int>(rows.size()) / 2;
      const int half = kMaxViewport / 2;
      int begin = std::max(0, mid - half);
      int end = std::min(static_cast<int>(rows.size()), begin + kMaxViewport);
      begin = std::max(0, end - kMaxViewport);
      rows = std::vector<int>(rows.begin() + begin, rows.begin() + end);
    }

    // Drop D-Bus backlog from previous scroll positions. Ready icons stay in the
    // model; clear *all* Pending so request_rows can re-queue the current
    // viewport (it skips paths already marked Pending/Ready).
    thumbs_.cancel_all();
    if (model_ != nullptr) {
      model_->clear_pending_thumbnails();
    }

    const bool need_dir = thumbs_.request_rows(
        visible, rows, model_,
        [this](const fs::Location& archive_root) -> std::optional<std::filesystem::path> {
          return archive_manager_.extracted_root(archive_root);
        });
    if (need_dir) {
      schedule_directory_thumbnails_low_priority();
    }
    // Re-assert the ActivityMonitor task after cancel_all/clear_pending so we do
    // not sit on Idle until the first Ready/Failed signal arrives.
    update_status_selection();
  });
}

void MainWindow::on_thumbnail_ready(const fs::Location& location, const QString& path)
{
  QPixmap pix(path);
  if (pix.isNull()) {
    qWarning().noquote() << QStringLiteral(
        "dirtoo: thumbnail cache file unreadable or empty: %1 (for %2)")
                                .arg(path, QString::fromStdString(location.as_path().string()));
    return;
  }
  QString key = QString::fromStdString(location.as_path().string());
  if (const auto it = thumbs_.aliases().constFind(key); it != thumbs_.aliases().cend()) {
    key = it.value();
  }
  if (model_ != nullptr) {
    model_->set_thumbnail(key, QIcon(pix));
    // Always refresh so the last pending → ready transition clears the
    // ActivityMonitor "Thumbnails" task (previously skipped when pending hit 0).
    update_status_selection();
  }
}

void MainWindow::on_thumbnail_failed(const fs::Location& location, const QString& message)
{
  QString key = QString::fromStdString(location.as_path().string());
  if (const auto it = thumbs_.aliases().constFind(key); it != thumbs_.aliases().cend()) {
    key = it.value();
  }
  // Always surface the reason on stderr (not gated on --verbose/--debug).
  if (!message.isEmpty()) {
    qWarning().noquote() << QStringLiteral("dirtoo: thumbnail failed for %1: %2")
                                .arg(key, message);
  } else {
    qWarning().noquote() << QStringLiteral("dirtoo: thumbnail failed for %1 (no reason given)")
                                .arg(key);
  }

  // Extension was wrong (e.g. .png that is JPEG): one content-MIME re-queue.
  if (thumbs_.try_content_mime_retry(location)) {
    return;
  }

  if (model_ == nullptr) {
    return;
  }
  // Only keep a visible error/caution badge for types that normally produce
  // thumbnails. Text, binaries, etc. often fail the D-Bus thumbnailer — fall
  // back to the system icon without a warning sticker (dirtoo-py Unavailable).
  // Directory montages report their own failures via DirectoryThumbnailWorker.
  const QString mime = mime_from_extension(location.as_path());
  const bool expect_thumb =
      mime_expects_thumbnail(mime) || message.contains(QLatin1String("directory thumbnail"));
  if (expect_thumb) {
    model_->set_thumbnail_failed(key);
  } else {
    model_->clear_thumbnail(key);
  }
  // Always refresh so finishing the last pending item clears ActivityMonitor.
  update_status_selection();
}

void MainWindow::on_reload_thumbnails()
{
  if (model_ == nullptr) {
    return;
  }
  thumbs_.cancel_all();

  auto selected = selected_fileinfos();
  std::vector<fs::FileInfo> targets;
  if (selected.empty()) {
    for (const auto& fi : collection_.visible_items()) {
      targets.push_back(fi);
    }
    model_->clear_thumbnails();
  } else {
    targets = std::move(selected);
    for (const auto& fi : targets) {
      if (!fi.path().empty()) {
        model_->clear_thumbnail(QString::fromStdString(fi.path().string()));
      }
    }
  }

  const int n = thumbs_.force_regenerate(targets, model_);
  set_status(QStringLiteral("Regenerating %1 thumbnail(s) from scratch…").arg(n));
  update_status_selection();
}



void MainWindow::on_make_directory_thumbnails()
{
  if (thumbs_.dir_worker() == nullptr) {
    return;
  }
  QStringList dirs;
  auto selected = selected_fileinfos();
  if (selected.empty()) {
    // All visible directories (include synthetic search hits with real paths)
    for (const auto& fi : collection_.visible_items()) {
      if (fi.is_directory() && !fi.path().empty()) {
        dirs << QString::fromStdString(fi.path().string());
      }
    }
  } else {
    for (const auto& fi : selected) {
      if (fi.is_directory() && !fi.path().empty()) {
        dirs << QString::fromStdString(fi.path().string());
      }
    }
  }
  if (dirs.isEmpty()) {
    set_status(QStringLiteral("No directories to thumbnail"));
    return;
  }
  set_status(QStringLiteral("Building %1 directory thumbnail(s)…").arg(dirs.size()));
  for (const QString& d : dirs) {
    if (model_ != nullptr) {
      model_->set_thumbnail_pending(d);
    }
  }
  QMetaObject::invokeMethod(thumbs_.dir_worker(), "generate", Qt::QueuedConnection,
                            Q_ARG(QStringList, dirs));
}

void MainWindow::on_prepare_thumbnails()
{
  // Full-directory warm-up (not just the viewport): queue Thumbnailer1 for every
  // listed item and kick MediaMetaCache probes so media columns / sorts can fill.
  // Prefer the full collection (unfiltered) so Prepare matches "whole directory".
  const auto& items = collection_.items();
  if (items.empty()) {
    set_status(QStringLiteral("Nothing to prepare"));
    return;
  }

  // Media metadata first (ffprobe/pdf/…) — independent of D-Bus thumbnail queue.
  const std::uint64_t gen = filter::MediaMetaCache::instance().generation();
  int meta_queued = 0;
  for (const auto& fi : items) {
    if (fi.is_directory() || fi.path().empty()) {
      continue;
    }
    if (filter::MediaMetaCache::instance().try_get(fi.path()).has_value()
        || filter::MediaMetaCache::instance().is_negative(fi.path())) {
      continue;
    }
    // No ready callback: next paint/data() hits try_get and shows columns.
    filter::MediaMetaCache::instance().request(fi.path(), gen, {});
    ++meta_queued;
  }

  // Chunk Thumbnailer1 requests so a 10k-file folder does not build one giant
  // D-Bus Queue message. Scroll-driven cancel_all can still interrupt this.
  constexpr int kChunk = 80;
  const int total = static_cast<int>(items.size());
  auto queue_chunk = std::make_shared<std::function<void(int)>>();
  *queue_chunk = [this, total, kChunk, queue_chunk](int start) {
    if (start >= total) {
      set_status(QStringLiteral("Prepare finished — %1 items queued").arg(total));
      update_status_selection();
      return;
    }
    const auto& items_now = collection_.items();
    if (static_cast<int>(items_now.size()) != total) {
      // Listing changed under us (navigate / filter); stop.
      return;
    }
    std::vector<int> rows;
    const int end = std::min(start + kChunk, total);
    rows.reserve(static_cast<std::size_t>(end - start));
    for (int i = start; i < end; ++i) {
      rows.push_back(i);
    }
    const bool need_dir = thumbs_.request_rows(
        items_now, rows, model_,
        [this](const fs::Location& archive_root) -> std::optional<std::filesystem::path> {
          return archive_manager_.extracted_root(archive_root);
        });
    if (need_dir) {
      schedule_directory_thumbnails_low_priority();
    }
    set_status(QStringLiteral("Preparing thumbnails %1/%2…").arg(end).arg(total));
    update_status_selection();
    QTimer::singleShot(30, this, [queue_chunk, end] { (*queue_chunk)(end); });
  };
  (*queue_chunk)(0);

  set_status(QStringLiteral("Preparing %1 item(s)… (%2 media probes)")
                 .arg(total)
                 .arg(meta_queued));
  update_status_selection();
}

} // namespace dirtoo::app

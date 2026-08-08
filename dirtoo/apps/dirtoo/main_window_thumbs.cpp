// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"
#include "directory_thumbnail_worker.hpp"

#include "archive_member_cache.hpp"
#include "dirtoo/archive/archive_index.hpp"
#include "dirtoo/thumbnail/thumbnailer.hpp"
#include <QDebug>
#include <QFile>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QGraphicsItem>
#include <filesystem>
#include <QIcon>
#include <QPixmap>

namespace dirtoo::app {

void MainWindow::cancel_all_thumbnails()
{
  thumbs_.cancel_all();
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
      if (!fi.is_directory() || fi.is_synthetic()) {
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
  QTimer::singleShot(80, this, [this] {
    const auto& visible = collection_.visible_items();
    if (visible.empty()) {
      return;
    }

    // Prefer viewport rows when the active view can report them; fall back to a
    // capped prefix so huge directories do not queue thousands of D-Bus jobs.
    std::vector<int> rows;
    if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
      const QRect vp = graphics_view_->viewport()->rect().adjusted(-64, -64, 64, 64);
      const QRectF scene_rect = graphics_view_->mapToScene(vp).boundingRect();
      for (QGraphicsItem* gi : graphics_view_->scene()->items(scene_rect)) {
        if (auto* item = qgraphicsitem_cast<GraphicsFileItem*>(gi)) {
          rows.push_back(item->row());
        }
      }
      std::sort(rows.begin(), rows.end());
      rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    } else if (QAbstractItemView* view = current_view()) {
      const QRect vp = view->viewport()->rect();
      // Walk the viewport by approximate row step so Detail/List only queue
      // thumbnails for on-screen rows (plus a small pad), not a huge range.
      const int step = std::max(8, view->sizeHintForRow(0) > 0 ? view->sizeHintForRow(0) : 24);
      int first = model_->rowCount();
      int last = -1;
      for (int y = 0; y < vp.height(); y += step) {
        const QModelIndex idx = view->indexAt(QPoint(4, y));
        if (!idx.isValid()) {
          continue;
        }
        first = std::min(first, idx.row());
        last = std::max(last, idx.row());
      }
      if (last < 0) {
        const QModelIndex top = view->indexAt(vp.topLeft() + QPoint(4, 4));
        const QModelIndex bot = view->indexAt(vp.bottomLeft() + QPoint(4, -4));
        first = top.isValid() ? top.row() : 0;
        last = bot.isValid() ? bot.row() : first;
      }
      if (last < first) {
        std::swap(first, last);
      }
      first = std::max(0, first - 4);
      last = std::min(model_->rowCount() - 1, last + 8);
      for (int r = first; r <= last; ++r) {
        rows.push_back(r);
      }
    }

    constexpr int kMaxBatch = 64;
    if (rows.empty()) {
      const int n = std::min(static_cast<int>(visible.size()), kMaxBatch);
      rows.reserve(static_cast<std::size_t>(n));
      for (int r = 0; r < n; ++r) {
        rows.push_back(r);
      }
    } else if (static_cast<int>(rows.size()) > kMaxBatch) {
      rows.resize(static_cast<std::size_t>(kMaxBatch));
    }

    std::vector<fs::Location> locs;
    QStringList mimes;
    locs.reserve(rows.size());
    mimes.reserve(static_cast<int>(rows.size()));
    bool any_dir_without_cache = false;
    for (int r : rows) {
      if (r < 0 || static_cast<std::size_t>(r) >= visible.size()) {
        continue;
      }
      const auto& fi = visible[static_cast<std::size_t>(r)];
      if (fi.is_directory()) {
        // Use an existing XDG/cache montage if present; auto-generate is deferred
        // via schedule_directory_thumbnails_low_priority().
        const QString cache = thumbnail::Thumbnailer::cache_path_for(fi.location(),
                                                                     QStringLiteral("large"));
        if (QFile::exists(cache) && model_ != nullptr) {
          model_->set_thumbnail(QString::fromStdString(fi.path().string()), QIcon(cache));
        } else {
          any_dir_without_cache = true;
        }
        continue;
      }
      // Search hits use synthetic FileInfo (no GUI-thread stat) but still refer to
      // real local paths — thumb them. Archive members are handled below.
      const QString model_key = QString::fromStdString(fi.path().string());
      if (model_ != nullptr) {
        if (model_->thumbnail_status(model_key) == ThumbnailStatus::Ready
            || model_->thumbnail_status(model_key) == ThumbnailStatus::Pending) {
          continue;
        }
        model_->set_thumbnail_pending(model_key);
      }
      static QMimeDatabase mime_db;
      const QString name_for_mime = QString::fromStdString(fi.basename());
      const QMimeType mt = mime_db.mimeTypeForFile(name_for_mime, QMimeDatabase::MatchExtension);
      const QString mime =
          mt.isValid() ? mt.name() : QStringLiteral("application/octet-stream");

      if (fi.location().is_archive()) {
        std::filesystem::path real;
        const fs::Location archive_root =
            fs::Location::from_archive(fi.location().as_path(), {});
        if (const auto root = archive_manager_.extracted_root(archive_root)) {
          real = *root / fi.location().entry_path();
        }
        if (real.empty() || !std::filesystem::exists(real)) {
          // Extract single member off the GUI thread, then ask Thumbnailer1.
          const auto archive_file = fi.location().as_path();
          const auto member = fi.location().entry_path();
          const QString key = model_key;
          const QString mime_copy = mime;
          const auto cache_root = archive_member_cache_root("dirtoo-archive-thumbs");
          (void)QtConcurrent::run([this, archive_file, member, key, mime_copy, cache_root]() {
            const auto dest_dir = archive_member_dest_dir(cache_root, archive_file);
            auto extracted =
                ensure_archive_member_extracted(archive_file, member, dest_dir);
            const bool ok = extracted.has_value();
            const std::filesystem::path out_path = ok ? *extracted : std::filesystem::path{};
            QMetaObject::invokeMethod(this, [this, ok, out_path, key, mime_copy]() {
              if (!ok) {
                if (model_ != nullptr) {
                  model_->set_thumbnail_failed(key);
                }
                return;
              }
              const QString real_path = QString::fromStdString(out_path.string());
              thumbs_.aliases().insert(real_path, key);
              thumbs_.thumbnailer().request(fs::Location::from_path(out_path), mime_copy,
                                   QStringLiteral("large"));
            }, Qt::QueuedConnection);
          });
          continue;
        }
        const QString real_path = QString::fromStdString(real.string());
        thumbs_.aliases().insert(real_path, model_key);
        locs.push_back(fs::Location::from_path(real));
        mimes.push_back(mime);
        continue;
      }

      locs.push_back(fi.location());
      mimes.push_back(mime);
    }
    if (!locs.empty()) {
      qDebug().noquote() << QStringLiteral("thumbnails: requesting %1 (viewport/batch)")
                                .arg(locs.size());
      thumbs_.request_many(locs, mimes);
    }
    if (any_dir_without_cache) {
      schedule_directory_thumbnails_low_priority();
    }
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
  }
  // Refresh "thumbs N/M" in the status bar (cheap; only when pending remain).
  if (model_ != nullptr && model_->thumbnail_counts().pending > 0) {
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
  if (model_ == nullptr) {
    return;
  }
  // Only keep a visible error/caution badge for types that normally produce
  // thumbnails. Text, binaries, etc. often fail the D-Bus thumbnailer — fall
  // back to the system icon without a warning sticker (dirtoo-py Unavailable).
  // Directory montages report their own failures via DirectoryThumbnailWorker.
  const QString name = QString::fromStdString(location.basename());
  static QMimeDatabase mime_db;
  const QMimeType mt = mime_db.mimeTypeForFile(name, QMimeDatabase::MatchExtension);
  const QString mime = mt.isValid() ? mt.name() : QString{};
  const bool expect_thumb =
      mime.startsWith(QLatin1String("image/")) || mime.startsWith(QLatin1String("video/"))
      || mime == QLatin1String("application/pdf")
      || mime.contains(QLatin1String("opendocument"))
      || mime.contains(QLatin1String("officedocument"))
      || message.contains(QLatin1String("directory thumbnail"));
  if (expect_thumb) {
    model_->set_thumbnail_failed(key);
  } else {
    model_->clear_thumbnail(key);
  }
  if (model_->thumbnail_counts().pending > 0) {
    update_status_selection();
  }
}

void MainWindow::on_reload_thumbnails()
{
  if (model_ == nullptr) {
    return;
  }
  thumbs_.cancel_all();
  auto selected = selected_fileinfos();
  if (selected.empty()) {
    // Reload all visible
    model_->clear_thumbnails();
    request_thumbnails_for_visible();
    set_status(QStringLiteral("Reloading thumbnails for visible items…"));
    return;
  }
  for (const auto& fi : selected) {
    if (fi.is_directory()) {
      continue;
    }
    // Synthetic search hits still have real paths; only skip empty.
    if (fi.path().empty()) {
      continue;
    }
    model_->clear_thumbnail(QString::fromStdString(fi.path().string()));
  }
  // Re-request only selection by temporarily using visible-style request on those paths
  QMimeDatabase mime_db;
  std::vector<fs::Location> locs;
  QStringList mimes;
  for (const auto& fi : selected) {
    if (fi.is_directory() || fi.path().empty() || location_.is_archive()) {
      continue;
    }
    const QString path = QString::fromStdString(fi.path().string());
    model_->set_thumbnail_pending(path);
    locs.push_back(fi.location());
    mimes.push_back(mime_db.mimeTypeForFile(path).name());
  }
  if (!locs.empty()) {
    thumbs_.request_many(locs, mimes);
  }
  set_status(QStringLiteral("Reloading %1 thumbnail(s)…").arg(locs.size()));
}


void MainWindow::on_make_directory_thumbnails()
{
  if (thumbs_.dir_worker() == nullptr) {
    return;
  }
  QStringList dirs;
  auto selected = selected_fileinfos();
  if (selected.empty()) {
    // All visible directories
    for (const auto& fi : collection_.visible_items()) {
      if (fi.is_directory() && !fi.is_synthetic()) {
        dirs << QString::fromStdString(fi.path().string());
      }
    }
  } else {
    for (const auto& fi : selected) {
      if (fi.is_directory() && !fi.is_synthetic()) {
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
  if (view_mode_ == ViewMode::Detail) {
    // Still useful: switch-less prepare for when user opens icons next
  }
  request_thumbnails_for_visible();
  set_status(QStringLiteral("Preparing thumbnails for visible items…"));
}


} // namespace dirtoo::app

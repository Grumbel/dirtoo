// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "file_list_model.hpp"
#include "opened_files_store.hpp"
#include <QDateTime>

#include <QIcon>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QMetaObject>
#include <QThreadPool>
#include <QTimer>

#include <filesystem>
#include <system_error>

namespace dirtoo::app {

namespace {

QFileIconProvider& icon_provider()
{
  static QFileIconProvider provider;
  return provider;
}

} // namespace

void FileListModel::set_thumbnail(const QString& path, const QIcon& icon)
{
  thumbnails_.insert(path, icon);
  thumbnail_status_.insert(path, ThumbnailStatus::Ready);
  thumbnail_pending_since_.remove(path);
  // Do not clear the "new" badge when a thumbnail arrives — Python keeps _new
  // until the directory is reloaded / the item is replaced.
  emit_path_changed(path);
}

void FileListModel::set_thumbnail_pending(const QString& path)
{
  if (thumbnail_status_.value(path, ThumbnailStatus::None) == ThumbnailStatus::Ready) {
    return;
  }
  thumbnail_status_.insert(path, ThumbnailStatus::Pending);
  if (!thumbnail_pending_since_.contains(path)) {
    thumbnail_pending_since_.insert(path, QDateTime::currentMSecsSinceEpoch());
  }
  emit_path_changed(path);
}

void FileListModel::set_thumbnail_failed(const QString& path)
{
  thumbnail_status_.insert(path, ThumbnailStatus::Failed);
  thumbnail_pending_since_.remove(path);
  emit_path_changed(path);
}

void FileListModel::flash_launch(const QString& path)
{
  if (path.isEmpty()) {
    return;
  }
  // Invalidate any previous blink sequence (navigate / re-open).
  clear_launch_flash();
  const quint64 gen = ++launch_flash_generation_;

  // Short blink so double-click feedback is obvious while the external app starts.
  // Nested singleShots capture @p gen; clear_launch_flash bumps the generation so
  // timers from a prior list still fire but do not paint the wrong row.
  auto step = [this, path, gen](bool on) {
    if (gen != launch_flash_generation_) {
      return false;
    }
    if (on) {
      launch_flash_paths_.insert(path);
    } else {
      launch_flash_paths_.remove(path);
    }
    emit_path_changed(path);
    return true;
  };

  step(true);
  QTimer::singleShot(90, this, [this, path, gen, step] {
    if (!step(false)) {
      return;
    }
    QTimer::singleShot(90, this, [this, path, gen, step] {
      if (!step(true)) {
        return;
      }
      QTimer::singleShot(90, this, [this, path, gen, step] {
        if (!step(false)) {
          return;
        }
        QTimer::singleShot(90, this, [this, path, gen, step] {
          if (!step(true)) {
            return;
          }
          QTimer::singleShot(140, this, [this, path, gen, step] {
            (void)step(false);
            (void)gen;
            (void)path;
          });
        });
      });
    });
  });
}

void FileListModel::clear_launch_flash()
{
  ++launch_flash_generation_;
  if (launch_flash_paths_.isEmpty()) {
    return;
  }
  const QSet<QString> paths = launch_flash_paths_;
  launch_flash_paths_.clear();
  for (const QString& p : paths) {
    emit_path_changed(p);
  }
}

bool FileListModel::is_launch_flash(const QString& path) const
{
  return launch_flash_paths_.contains(path);
}

void FileListModel::mark_new(const QString& path)
{
  new_paths_.insert(path);
  emit_path_changed(path);
}

void FileListModel::clear_new_marks()
{
  if (new_paths_.isEmpty()) {
    return;
  }
  new_paths_.clear();
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {IsNewRole});
  }
}

void FileListModel::prune_new_marks(const QSet<QString>& keep_paths)
{
  if (new_paths_.isEmpty()) {
    return;
  }
  QSet<QString> removed;
  for (const QString& p : new_paths_) {
    if (!keep_paths.contains(p)) {
      removed.insert(p);
    }
  }
  if (removed.isEmpty()) {
    return;
  }
  for (const QString& p : removed) {
    new_paths_.remove(p);
  }
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {IsNewRole});
  }
}

ThumbnailStatus FileListModel::thumbnail_status(const QString& path) const
{
  return thumbnail_status_.value(path, ThumbnailStatus::None);
}

FileListModel::ThumbnailCounts FileListModel::thumbnail_counts() const
{
  ThumbnailCounts c;
  for (auto it = thumbnail_status_.constBegin(); it != thumbnail_status_.constEnd(); ++it) {
    switch (it.value()) {
    case ThumbnailStatus::Pending:
      ++c.pending;
      break;
    case ThumbnailStatus::Ready:
      ++c.ready;
      break;
    case ThumbnailStatus::Failed:
      ++c.failed;
      break;
    default:
      break;
    }
  }
  return c;
}

bool FileListModel::is_new(const QString& path) const
{
  return new_paths_.contains(path);
}

bool FileListModel::is_opened(const QString& path) const
{
  return opened_files_store().is_opened(path);
}

void FileListModel::set_show_opened_state(bool on)
{
  if (show_opened_state_ == on) {
    return;
  }
  show_opened_state_ = on;
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1),
                     {IsOpenedRole, Qt::BackgroundRole});
  }
}

void FileListModel::set_unopened_highlight_color(const QColor& c)
{
  if (!c.isValid()) {
    return;
  }
  if (unopened_highlight_color_ == c) {
    return;
  }
  unopened_highlight_color_ = c;
  if (show_opened_state_ && rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1),
                     {IsOpenedRole, Qt::BackgroundRole});
  }
}

void FileListModel::notify_opened_changed(const QStringList& paths)
{
  for (const QString& p : paths) {
    emit_path_changed(p);
  }
}

void FileListModel::refresh_opened_roles()
{
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1),
                     {IsOpenedRole, Qt::BackgroundRole});
  }
}

void FileListModel::clear_child_counts()
{
  child_counts_.clear();
  child_count_pending_.clear();
}

void FileListModel::request_child_count(const QString& path)
{
  if (path.isEmpty() || child_counts_.contains(path) || child_count_pending_.contains(path)) {
    return;
  }
  child_count_pending_.insert(path);
  child_counts_.insert(path, -1);
  // Offload readdir to a worker thread; never run on the GUI thread.
  const QString path_copy = path;
  QThreadPool::globalInstance()->start([this, path_copy] {
    qint64 n = 0;
    std::error_code ec;
    const auto opts = std::filesystem::directory_options::skip_permission_denied;
    for (const auto& entry :
         std::filesystem::directory_iterator(path_copy.toStdString(), opts, ec)) {
      (void)entry;
      if (ec) {
        ec.clear();
        continue;
      }
      ++n;
    }
    QMetaObject::invokeMethod(this, "on_child_count_ready", Qt::QueuedConnection,
                              Q_ARG(QString, path_copy), Q_ARG(qint64, n));
  });
}

void FileListModel::on_child_count_ready(const QString& path, qint64 count)
{
  child_count_pending_.remove(path);
  child_counts_.insert(path, count);
  emit_path_changed(path);
}

void FileListModel::set_child_count(const QString& path, qint64 count)
{
  child_count_pending_.remove(path);
  child_counts_.insert(path, count);
  emit_path_changed(path);
}


void FileListModel::clear_thumbnail(const QString& path)
{
  thumbnails_.remove(path);
  thumbnail_status_.remove(path);
  thumbnail_pending_since_.remove(path);
  emit_path_changed(path);
}

void FileListModel::clear_pending_thumbnails()
{
  QStringList pending;
  for (auto it = thumbnail_status_.constBegin(); it != thumbnail_status_.constEnd(); ++it) {
    if (it.value() == ThumbnailStatus::Pending) {
      pending << it.key();
    }
  }
  if (pending.isEmpty()) {
    return;
  }
  for (const QString& path : pending) {
    thumbnail_status_.remove(path);
    thumbnail_pending_since_.remove(path);
  }
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {ThumbnailStatusRole});
  }
}

int FileListModel::clear_stale_pending_thumbnails(qint64 max_age_ms)
{
  if (max_age_ms <= 0) {
    return 0;
  }
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  QStringList stale;
  for (auto it = thumbnail_pending_since_.constBegin(); it != thumbnail_pending_since_.constEnd();
       ++it) {
    if (thumbnail_status_.value(it.key(), ThumbnailStatus::None) != ThumbnailStatus::Pending) {
      continue;
    }
    if (now - it.value() >= max_age_ms) {
      stale << it.key();
    }
  }
  for (const QString& path : stale) {
    thumbnail_status_.remove(path);
    thumbnail_pending_since_.remove(path);
  }
  if (!stale.isEmpty() && rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {ThumbnailStatusRole});
  }
  return stale.size();
}

void FileListModel::clear_thumbnails()
{
  if (thumbnails_.isEmpty() && thumbnail_status_.isEmpty()) {
    return;
  }
  thumbnails_.clear();
  thumbnail_status_.clear();
  thumbnail_pending_since_.clear();
  // Child counts are location-specific; cleared separately on navigation.
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0),
                     {Qt::DecorationRole, ThumbnailStatusRole});
  }
}

void FileListModel::set_crop_thumbnails(bool crop)
{
  if (crop_thumbnails_ == crop) {
    return;
  }
  crop_thumbnails_ = crop;
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {Qt::DecorationRole});
  }
}


QIcon FileListModel::icon_for(const fs::FileInfo& fi) const
{
  const QString path = QString::fromStdString(fi.path().string());
  const auto it = thumbnails_.constFind(path);
  if (it != thumbnails_.constEnd()) {
    // Thumbnail path: symlink emblem is painted by paint_tile_status_overlays.
    return it.value();
  }
  QIcon base;
  if (fi.is_synthetic()) {
    base = icon_provider().icon(fi.is_directory() ? QFileIconProvider::Folder
                                                  : QFileIconProvider::File);
  } else {
    base = icon_provider().icon(QFileInfo(path));
  }
  if (!fi.is_symlink() || base.isNull()) {
    return base;
  }
  // Detail/List modes only see DecorationRole — bake a small symlink badge
  // into the icon so links are recognizable without Graphics overlays.
  QIcon badged;
  for (const QSize& sz : {QSize(16, 16), QSize(32, 32), QSize(48, 48), QSize(64, 64)}) {
    QPixmap pm = base.pixmap(sz);
    if (pm.isNull()) {
      continue;
    }
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const int e = std::max(8, sz.width() / 3);
    QRect r(1, sz.height() - e - 1, e, e);
    static const QIcon theme = QIcon::fromTheme(QStringLiteral("emblem-symbolic-link"));
    if (!theme.isNull()) {
      theme.paint(&painter, r, Qt::AlignCenter);
    } else {
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(255, 255, 255, 220));
      painter.drawEllipse(r);
      painter.setPen(QPen(QColor(30, 90, 200), 1.5));
      painter.drawArc(r.adjusted(2, 2, -2, -2), 40 * 16, 200 * 16);
    }
    painter.end();
    badged.addPixmap(pm);
  }
  return badged.isNull() ? base : badged;
}


} // namespace dirtoo::app

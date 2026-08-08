// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "file_list_model.hpp"

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
  emit_path_changed(path);
}

void FileListModel::set_thumbnail_failed(const QString& path)
{
  thumbnail_status_.insert(path, ThumbnailStatus::Failed);
  emit_path_changed(path);
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
  emit_path_changed(path);
}

void FileListModel::clear_thumbnails()
{
  if (thumbnails_.isEmpty() && thumbnail_status_.isEmpty()) {
    return;
  }
  thumbnails_.clear();
  thumbnail_status_.clear();
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

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thumbnail_coordinator.hpp"

#include "archive_member_cache.hpp"
#include "file_list_model.hpp"
#include "dirtoo/fs/location.hpp"
#include "dirtoo/thumbnail/thumbnailer.hpp"

#include <QDebug>
#include <QFile>
#include <QIcon>
#include <QMimeDatabase>
#include <QtConcurrent>

#include <filesystem>

namespace dirtoo::app {

ThumbnailCoordinator::ThumbnailCoordinator(QObject* parent)
    : QObject(parent)
{
}

ThumbnailCoordinator::~ThumbnailCoordinator()
{
  shutdown();
}

void ThumbnailCoordinator::cancel_all()
{
  thumbnailer_.cancel_all();
}

void ThumbnailCoordinator::clear_aliases()
{
  thumb_alias_.clear();
}

void ThumbnailCoordinator::request_many(const std::vector<fs::Location>& locs,
                                       const QStringList& mimes)
{
  if (locs.empty()) {
    return;
  }
  thumbnailer_.request_many(locs, mimes, QStringLiteral("large"));
}

bool ThumbnailCoordinator::request_rows(const std::vector<fs::FileInfo>& visible,
                                        const std::vector<int>& rows, FileListModel* model,
                                        const ExtractedRootFn& extracted_root)
{
  std::vector<fs::Location> locs;
  QStringList mimes;
  locs.reserve(rows.size());
  mimes.reserve(static_cast<int>(rows.size()));
  bool any_dir_without_cache = false;
  static QMimeDatabase mime_db;

  for (int r : rows) {
    if (r < 0 || static_cast<std::size_t>(r) >= visible.size()) {
      continue;
    }
    const auto& fi = visible[static_cast<std::size_t>(r)];
    if (fi.is_directory()) {
      // Use an existing XDG/cache montage if present; auto-generate is deferred
      // by the caller via schedule_directory_thumbnails_low_priority().
      const QString cache =
          thumbnail::Thumbnailer::cache_path_for(fi.location(), QStringLiteral("large"));
      if (QFile::exists(cache) && model != nullptr) {
        model->set_thumbnail(QString::fromStdString(fi.path().string()), QIcon(cache));
      } else {
        any_dir_without_cache = true;
      }
      continue;
    }
    // Search hits use synthetic FileInfo (no GUI-thread stat) but still refer to
    // real local paths — thumb them. Archive members are handled below.
    const QString model_key = QString::fromStdString(fi.path().string());
    if (model != nullptr) {
      if (model->thumbnail_status(model_key) == ThumbnailStatus::Ready
          || model->thumbnail_status(model_key) == ThumbnailStatus::Pending) {
        continue;
      }
      model->set_thumbnail_pending(model_key);
    }
    const QString name_for_mime = QString::fromStdString(fi.basename());
    const QMimeType mt = mime_db.mimeTypeForFile(name_for_mime, QMimeDatabase::MatchExtension);
    const QString mime =
        mt.isValid() ? mt.name() : QStringLiteral("application/octet-stream");

    if (fi.location().is_archive()) {
      const auto archive_file = fi.location().as_path();
      const auto member = fi.location().entry_path();
      const auto cache_root = archive_member_cache_root("dirtoo-archive-thumbs");
      const auto dest_dir = archive_member_dest_dir(cache_root, archive_file);

      std::filesystem::path real;
      // Prefer shared extract cache (TagJob / prior thumbs) before full archive open.
      {
        std::error_code ec;
        const auto cached = dest_dir / member;
        if (!member.empty() && std::filesystem::is_regular_file(cached, ec) && !ec) {
          real = cached;
        }
      }
      if (real.empty() && extracted_root) {
        const fs::Location archive_root = fs::Location::from_archive(fi.location().as_path(), {});
        if (const auto root = extracted_root(archive_root)) {
          real = *root / member;
        }
      }
      if (real.empty() || !std::filesystem::exists(real)) {
        // Extract single member off the GUI thread, then ask Thumbnailer1.
        const QString key = model_key;
        const QString mime_copy = mime;
        (void)QtConcurrent::run([this, archive_file, member, key, mime_copy, dest_dir, model]() {
          auto extracted = ensure_archive_member_extracted(archive_file, member, dest_dir);
          const bool ok = extracted.has_value();
          const std::filesystem::path out_path = ok ? *extracted : std::filesystem::path{};
          QMetaObject::invokeMethod(
              this,
              [this, ok, out_path, key, mime_copy, model]() {
                if (!ok) {
                  if (model != nullptr) {
                    model->set_thumbnail_failed(key);
                  }
                  return;
                }
                const QString real_path = QString::fromStdString(out_path.string());
                thumb_alias_.insert(real_path, key);
                thumbnailer_.request(fs::Location::from_path(out_path), mime_copy,
                                     QStringLiteral("large"));
              },
              Qt::QueuedConnection);
        });
        continue;
      }
      const QString real_path = QString::fromStdString(real.string());
      thumb_alias_.insert(real_path, model_key);
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
    request_many(locs, mimes);
  }
  return any_dir_without_cache;
}

void ThumbnailCoordinator::setup_dir_worker()
{
  if (dir_thumb_worker_ != nullptr) {
    return;
  }
  dir_thumb_worker_ = new DirectoryThumbnailWorker;
  dir_thumb_thread_ = new QThread(this);
  dir_thumb_worker_->moveToThread(dir_thumb_thread_);
  connect(dir_thumb_thread_, &QThread::finished, dir_thumb_worker_, &QObject::deleteLater);
  dir_thumb_thread_->start();
}

void ThumbnailCoordinator::shutdown()
{
  cancel_all();
  clear_aliases();
  if (dir_thumb_thread_ != nullptr) {
    dir_thumb_thread_->quit();
    dir_thumb_thread_->wait(3000);
    dir_thumb_thread_ = nullptr;
    dir_thumb_worker_ = nullptr;
  }
}

} // namespace dirtoo::app

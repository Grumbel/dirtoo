// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thumbnail_coordinator.hpp"
#include <QMetaObject>

#include "archive_member_cache.hpp"
#include "async_io.hpp"
#include "file_list_model.hpp"
#include "mime_util.hpp"
#include "dirtoo/fs/location.hpp"
#include "dirtoo/thumbnail/thumbnailer.hpp"

#include <QDebug>
#include <QFile>
#include <QIcon>
#include <QPointer>
#include <QtConcurrent>

#include <filesystem>

namespace dirtoo::app {

ThumbnailCoordinator::ThumbnailCoordinator(QObject* parent)
    : QObject(parent)
{
  // Thumbnailer1 client on its own thread: it stats every source file (slow
  // drives) and makes synchronous D-Bus calls (busy/activating service).
  thumb_thread_ = new QThread(this);
  thumb_thread_->setObjectName(QStringLiteral("dirtoo-thumbnailer"));
  thumbnailer_ = new thumbnail::Thumbnailer;
  thumbnailer_->moveToThread(thumb_thread_);
  connect(thumb_thread_, &QThread::finished, thumbnailer_, &QObject::deleteLater);
  // Cross-thread → queued delivery on the GUI thread.
  connect(thumbnailer_, &thumbnail::Thumbnailer::thumbnail_ready, this,
          &ThumbnailCoordinator::on_thumbnailer_ready);
  connect(thumbnailer_, &thumbnail::Thumbnailer::thumbnail_failed, this,
          &ThumbnailCoordinator::on_thumbnailer_failed);
  thumb_thread_->start();
}

ThumbnailCoordinator::~ThumbnailCoordinator()
{
  shutdown();
}

void ThumbnailCoordinator::cancel_all()
{
  on_thumbnailer_thread([t = thumbnailer_] { t->cancel_all(); });
  content_mime_retried_.clear();
  octet_stream_retried_.clear();
  in_flight_ = 0;
}

void ThumbnailCoordinator::clear_aliases()
{
  thumb_alias_.clear();
  content_mime_retried_.clear();
  octet_stream_retried_.clear();
}

void ThumbnailCoordinator::clear_content_retries()
{
  content_mime_retried_.clear();
  octet_stream_retried_.clear();
}

void ThumbnailCoordinator::request_many(const std::vector<fs::Location>& locs,
                                       const QStringList& mimes, bool force)
{
  if (locs.empty()) {
    return;
  }
  // Every location ends in exactly one ready/failed signal (cache hits too),
  // which decrements in on_thumbnailer_ready/failed.
  in_flight_ += static_cast<int>(locs.size());
  on_thumbnailer_thread([t = thumbnailer_, locs, mimes, force] {
    t->request_many(locs, mimes, QStringLiteral("large"), force);
  });
}

void ThumbnailCoordinator::request_one(const fs::Location& location, const QString& mime)
{
  request_many({location}, {mime});
}

void ThumbnailCoordinator::on_thumbnailer_ready(const fs::Location& location, const QString& path)
{
  if (in_flight_ > 0) {
    --in_flight_;
  }
  emit thumbnail_ready(location, path);
}

void ThumbnailCoordinator::on_thumbnailer_failed(const fs::Location& location,
                                                 const QString& message)
{
  if (in_flight_ > 0) {
    --in_flight_;
  }
  // Re-try policy (once per path and session):
  //  1. content MIME differs from the extension guess (e.g. .png that is
  //     JPEG) → re-queue with the content MIME;
  //  2. otherwise → one application/octet-stream request (Hilbert / other
  //     binary map thumbnailers, if installed).
  // Deciding needs a stat and a content sniff — done on the I/O pool.
  std::filesystem::path path;
  try {
    path = location.as_path();
  } catch (...) {
  }
  const QString path_q = QString::fromStdString(path.string());
  const QString ext_mime = path.empty() ? QString() : mime_from_extension(path_q);
  const bool may_content = !path.empty() && !content_mime_retried_.contains(path_q);
  // Already asked as octet-stream (e.g. .bin) — another pass will not help.
  const bool may_octet = !path.empty() && !octet_stream_retried_.contains(path_q)
                         && ext_mime != QLatin1String("application/octet-stream");
  if (!may_content && !may_octet) {
    emit thumbnail_failed(location, message);
    return;
  }

  struct Probe {
    bool regular = false;
    QString content_mime;
  };
  ++in_flight_; // the decision is still outstanding work
  run_io(
      this,
      [path, path_q, may_content] {
        Probe p;
        std::error_code ec;
        p.regular = std::filesystem::is_regular_file(path, ec) && !ec;
        if (p.regular && may_content) {
          p.content_mime = mime_from_content(path_q);
        }
        return p;
      },
      [this, location, message, path_q, ext_mime, may_content, may_octet](const Probe& p) {
        if (in_flight_ > 0) {
          --in_flight_;
        }
        if (!p.regular) {
          emit thumbnail_failed(location, message);
          return;
        }
        if (may_content) {
          content_mime_retried_.insert(path_q); // avoid repeat work on re-fail
          if (!p.content_mime.isEmpty()
              && p.content_mime != QLatin1String("application/octet-stream")
              && !mime_equivalent_for_thumb(ext_mime, p.content_mime)) {
            qWarning().noquote()
                << QStringLiteral("dirtoo: thumbnail MIME retry %1: extension=%2 content=%3")
                       .arg(path_q, ext_mime, p.content_mime);
            request_many({location}, {p.content_mime}, /*force=*/true);
            return;
          }
        }
        if (may_octet) {
          octet_stream_retried_.insert(path_q);
          qInfo().noquote() << QStringLiteral(
                                   "dirtoo: thumbnail octet-stream fallback for %1 (was %2)")
                                   .arg(path_q, ext_mime.isEmpty() ? QStringLiteral("(unknown)")
                                                                   : ext_mime);
          request_many({location}, {QStringLiteral("application/octet-stream")},
                       /*force=*/true);
          return;
        }
        emit thumbnail_failed(location, message);
      });
}

void ThumbnailCoordinator::request_rows(const std::vector<fs::FileInfo>& visible,
                                        const std::vector<int>& rows, FileListModel* model,
                                        const ExtractedRootFn& extracted_root)
{
  std::vector<fs::Location> locs;
  QStringList mimes;
  locs.reserve(rows.size());
  mimes.reserve(static_cast<int>(rows.size()));
  std::vector<fs::Location> dirs_to_check;
  for (int r : rows) {
    if (r < 0 || static_cast<std::size_t>(r) >= visible.size()) {
      continue;
    }
    const auto& fi = visible[static_cast<std::size_t>(r)];
    if (fi.is_directory()) {
      // Use an existing XDG/cache montage if still fresh; auto-generate is
      // deferred by the receiver of directory_montages_needed(). Freshness
      // compares against the directory's mtime → stat → I/O pool.
      const QString key = QString::fromStdString(fi.path().string());
      if (model != nullptr
          && (model->thumbnail_status(key) == ThumbnailStatus::Ready
              || model->thumbnail_status(key) == ThumbnailStatus::Pending)) {
        continue;
      }
      if (!dir_cache_checks_.contains(key)) {
        dir_cache_checks_.insert(key);
        dirs_to_check.push_back(fi.location());
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
    // Extension of the full path (fast). Content retry happens on Thumbnailer failure.
    const QString mime = mime_for_thumbnail_fast(fi.path());

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
                request_one(fs::Location::from_path(out_path), mime_copy);
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

  if (!dirs_to_check.empty()) {
    QPointer<FileListModel> model_guard(model);
    run_io(
        this,
        [dirs_to_check] {
          std::vector<std::pair<QString, QString>> fresh; // dir path → cache png
          std::vector<QString> stale;
          for (const auto& loc : dirs_to_check) {
            const QString key = QString::fromStdString(loc.as_path().string());
            if (thumbnail::Thumbnailer::cache_is_fresh(loc, QStringLiteral("large"))) {
              fresh.emplace_back(key, thumbnail::Thumbnailer::cache_path_for(
                                          loc, QStringLiteral("large")));
            } else {
              stale.push_back(key);
            }
          }
          return std::make_pair(fresh, stale);
        },
        [this, model_guard](const std::pair<std::vector<std::pair<QString, QString>>,
                                            std::vector<QString>>& result) {
          for (const auto& [key, cache] : result.first) {
            dir_cache_checks_.remove(key);
            if (model_guard) {
              model_guard->set_thumbnail(key, QIcon(cache));
            }
          }
          for (const auto& key : result.second) {
            dir_cache_checks_.remove(key);
          }
          if (!result.second.empty()) {
            emit directory_montages_needed();
          }
        });
  }
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

namespace {

/// Stop a worker thread. A thread still blocked in I/O on a hung mount after
/// the grace period is detached and leaked: deleting a running QThread aborts
/// the process, and the stuck syscall cannot be interrupted anyway.
void stop_thread(QThread* thread)
{
  if (thread == nullptr) {
    return;
  }
  thread->quit();
  if (!thread->wait(3000)) {
    qWarning().noquote() << QStringLiteral("dirtoo: %1 still blocked at shutdown; detaching")
                                .arg(thread->objectName());
    thread->setParent(nullptr);
  }
}

} // namespace

void ThumbnailCoordinator::shutdown()
{
  cancel_all();
  clear_aliases();
  dir_cache_checks_.clear();
  if (thumb_thread_ != nullptr) {
    stop_thread(thumb_thread_);
    thumb_thread_ = nullptr;
    thumbnailer_ = nullptr; // deleteLater'd on thread finish (or leaked with it)
  }
  if (dir_thumb_thread_ != nullptr) {
    stop_thread(dir_thumb_thread_);
    dir_thumb_thread_ = nullptr;
    dir_thumb_worker_ = nullptr;
  }
}


int ThumbnailCoordinator::force_regenerate(const std::vector<fs::FileInfo>& targets,
                                           FileListModel* model)
{
  if (model == nullptr) {
    return 0;
  }
  std::vector<fs::Location> locs;
  QStringList mimes;
  QStringList dir_paths;
  locs.reserve(targets.size());
  mimes.reserve(static_cast<int>(targets.size()));

  for (const auto& fi : targets) {
    if (fi.path().empty()) {
      continue;
    }
    const QString path = QString::fromStdString(fi.path().string());

    if (fi.is_directory()) {
      (void)thumbnail::Thumbnailer::remove_cache_for(fi.location());
      // Synthetic search hits still have real paths; allow montage rebuild.
      if (!path.isEmpty()) {
        dir_paths << path;
        model->set_thumbnail_pending(path);
      }
      continue;
    }

    (void)thumbnail::Thumbnailer::remove_cache_for(fi.location());
    for (auto it = thumb_alias_.begin(); it != thumb_alias_.end();) {
      if (it.value() == path) {
        (void)thumbnail::Thumbnailer::remove_cache_for(
            fs::Location::from_path(std::filesystem::path(it.key().toStdString())));
        it = thumb_alias_.erase(it);
      } else {
        ++it;
      }
    }

    model->set_thumbnail_pending(path);

    // Extension of the full path (fast). Content retry happens on Thumbnailer failure.
    const QString mime = mime_for_thumbnail_fast(fi.path());

    if (fi.location().is_archive()) {
      const auto archive_file = fi.location().as_path();
      const auto member = fi.location().entry_path();
      const auto cache_root = archive_member_cache_root("dirtoo-archive-thumbs");
      const auto dest_dir = archive_member_dest_dir(cache_root, archive_file);
      std::error_code ec;
      const auto cached = dest_dir / member;
      if (!member.empty() && std::filesystem::is_regular_file(cached, ec) && !ec) {
        const auto real_loc = fs::Location::from_path(cached);
        (void)thumbnail::Thumbnailer::remove_cache_for(real_loc);
        thumb_alias_.insert(QString::fromStdString(cached.string()), path);
        locs.push_back(real_loc);
        mimes.push_back(mime);
      } else if (!member.empty()) {
        const QString key = path;
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
                (void)thumbnail::Thumbnailer::remove_cache_for(fs::Location::from_path(out_path));
                request_many({fs::Location::from_path(out_path)}, {mime_copy}, /*force=*/true);
              },
              Qt::QueuedConnection);
        });
      } else {
        model->set_thumbnail_failed(path);
      }
    } else {
      locs.push_back(fi.location());
      mimes.push_back(mime);
    }
  }

  if (!locs.empty()) {
    request_many(locs, mimes, /*force=*/true);
  }
  if (!dir_paths.isEmpty()) {
    setup_dir_worker();
    if (dir_thumb_worker_ != nullptr) {
      QMetaObject::invokeMethod(dir_thumb_worker_, "generate", Qt::QueuedConnection,
                                Q_ARG(QStringList, dir_paths));
    }
  }
  return static_cast<int>(locs.size()) + dir_paths.size();
}


} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/thumbnail/thumbnailer.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/fs/location.hpp"
#include "directory_thumbnail_worker.hpp"

#include <QHash>
#include <QSet>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>

#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

namespace dirtoo::app {

class FileListModel;

/// Owns D-Bus thumbnailer, archive path aliases, directory-montage worker, and
/// per-row request building (incl. archive member extract). Viewport row
/// discovery stays on MainWindow (needs views / view mode).
///
/// No filesystem I/O on the GUI thread: the Thumbnailer (which stats sources
/// and makes blocking D-Bus calls) lives on its own thread, directory montage
/// freshness checks and failure re-try probes run on the I/O pool.
class ThumbnailCoordinator : public QObject {
  Q_OBJECT
public:
  explicit ThumbnailCoordinator(QObject* parent = nullptr);
  ~ThumbnailCoordinator() override;

  [[nodiscard]] QHash<QString, QString>& aliases() { return thumb_alias_; }
  [[nodiscard]] const QHash<QString, QString>& aliases() const { return thumb_alias_; }

  [[nodiscard]] DirectoryThumbnailWorker* dir_worker() const { return dir_thumb_worker_; }

  void cancel_all();
  /// Outstanding thumbnail jobs we still expect (D-Bus queue + archive extract).
  /// Tracked here so the GUI does not depend on rebuilding dirtoo-thumbnail.
  [[nodiscard]] int in_flight_count() const noexcept { return in_flight_; }
  void clear_aliases();
  void clear_content_retries();
  void request_many(const std::vector<dirtoo::fs::Location>& locs, const QStringList& mimes,
                    bool force = false);

  /// Resolve archive-root extract path for a member (MainWindow/ArchiveManager).
  using ExtractedRootFn =
      std::function<std::optional<std::filesystem::path>(const dirtoo::fs::Location& archive_root)>;

  /// Queue thumbs for the given model rows of @p visible. Skips Ready/Pending,
  /// applies directory cache hits (checked off the GUI thread), and extracts
  /// archive members off the GUI thread when needed. Emits
  /// directory_montages_needed() when a directory lacks a fresh cache montage
  /// (receiver may schedule low-priority dir thumbs).
  void request_rows(const std::vector<dirtoo::fs::FileInfo>& visible,
                    const std::vector<int>& rows, FileListModel* model,
                    const ExtractedRootFn& extracted_root);

  /// Create directory-montage worker thread (idempotent).
  void setup_dir_worker();

  /// Reload Thumbnails: drop XDG cache, mark pending, force-queue Thumbnailer1
  /// (extracting archive members off-GUI when needed) and directory montages.
  /// @return number of file + directory work items scheduled.
  int force_regenerate(const std::vector<dirtoo::fs::FileInfo>& targets, FileListModel* model);

  /// Connect ready/failed (Thumbnailer1 after retries, and the dir worker) to
  /// receiver slots, plus directory_montages_needed.
  template <typename Receiver>
  void wire_ready_failed(Receiver* receiver,
                         void (Receiver::*ready)(const dirtoo::fs::Location&, const QString&),
                         void (Receiver::*failed)(const dirtoo::fs::Location&, const QString&))
  {
    connect(this, &ThumbnailCoordinator::thumbnail_ready, receiver, ready);
    connect(this, &ThumbnailCoordinator::thumbnail_failed, receiver, failed);
    if (dir_thumb_worker_ != nullptr) {
      connect(dir_thumb_worker_, &DirectoryThumbnailWorker::thumbnail_ready, receiver, ready);
      connect(dir_thumb_worker_, &DirectoryThumbnailWorker::thumbnail_failed, receiver, failed);
    }
  }

  void shutdown();

signals:
  void thumbnail_ready(const dirtoo::fs::Location& location, const QString& path);
  /// Final failure: emitted only after the content-MIME / octet-stream
  /// re-tries were considered.
  void thumbnail_failed(const dirtoo::fs::Location& location, const QString& message);
  /// A visible directory has no fresh cached montage.
  void directory_montages_needed();

private:
  void on_thumbnailer_ready(const dirtoo::fs::Location& location, const QString& path);
  void on_thumbnailer_failed(const dirtoo::fs::Location& location, const QString& message);
  /// Run @p fn on the Thumbnailer's thread.
  template <typename Fn>
  void on_thumbnailer_thread(Fn&& fn)
  {
    if (thumbnailer_ != nullptr) {
      QMetaObject::invokeMethod(thumbnailer_, std::forward<Fn>(fn), Qt::QueuedConnection);
    }
  }
  void request_one(const dirtoo::fs::Location& location, const QString& mime);

  QThread* thumb_thread_ = nullptr;
  thumbnail::Thumbnailer* thumbnailer_ = nullptr; ///< lives on thumb_thread_
  QHash<QString, QString> thumb_alias_;
  QThread* dir_thumb_thread_ = nullptr;
  DirectoryThumbnailWorker* dir_thumb_worker_ = nullptr;
  /// Absolute paths already re-queued with MatchContent for this session.
  QSet<QString> content_mime_retried_;
  QSet<QString> octet_stream_retried_;
  /// Directory paths whose montage freshness check is in flight.
  QSet<QString> dir_cache_checks_;
  /// Approximate outstanding work (request_many + archive extract paths).
  int in_flight_ = 0;
};

} // namespace dirtoo::app

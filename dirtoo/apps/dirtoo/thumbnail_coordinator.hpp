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
class ThumbnailCoordinator : public QObject {
  Q_OBJECT
public:
  explicit ThumbnailCoordinator(QObject* parent = nullptr);
  ~ThumbnailCoordinator() override;

  [[nodiscard]] thumbnail::Thumbnailer& thumbnailer() { return thumbnailer_; }
  [[nodiscard]] const thumbnail::Thumbnailer& thumbnailer() const { return thumbnailer_; }

  [[nodiscard]] QHash<QString, QString>& aliases() { return thumb_alias_; }
  [[nodiscard]] const QHash<QString, QString>& aliases() const { return thumb_alias_; }

  [[nodiscard]] DirectoryThumbnailWorker* dir_worker() const { return dir_thumb_worker_; }

  void cancel_all();
  [[nodiscard]] int in_flight_count() const { return thumbnailer_.in_flight_count(); }
  void clear_aliases();
  void clear_content_retries();
  void request_many(const std::vector<dirtoo::fs::Location>& locs, const QStringList& mimes,
                    bool force = false);

  /// After Thumbnailer1 fails: one content-MIME re-queue if magic differs from the
  /// extension guess (e.g. .png that is actually JPEG). Returns true if a retry
  /// was scheduled (caller should not mark the model failed yet).
  bool try_content_mime_retry(const dirtoo::fs::Location& location);

  /// Resolve archive-root extract path for a member (MainWindow/ArchiveManager).
  using ExtractedRootFn =
      std::function<std::optional<std::filesystem::path>(const dirtoo::fs::Location& archive_root)>;

  /// Queue thumbs for the given model rows of @p visible. Skips Ready/Pending,
  /// applies directory cache hits, and extracts archive members off the GUI
  /// thread when needed. Returns true if any directory lacked a cache montage
  /// (caller may schedule low-priority dir thumbs).
  bool request_rows(const std::vector<dirtoo::fs::FileInfo>& visible,
                    const std::vector<int>& rows, FileListModel* model,
                    const ExtractedRootFn& extracted_root);

  /// Create directory-montage worker thread (idempotent).
  void setup_dir_worker();

  /// Reload Thumbnails: drop XDG cache, mark pending, force-queue Thumbnailer1
  /// (extracting archive members off-GUI when needed) and directory montages.
  /// @return number of file + directory work items scheduled.
  int force_regenerate(const std::vector<dirtoo::fs::FileInfo>& targets, FileListModel* model);

  /// Connect Thumbnailer + dir worker ready/failed to receiver slots.
  template <typename Receiver>
  void wire_ready_failed(Receiver* receiver,
                         void (Receiver::*ready)(const dirtoo::fs::Location&, const QString&),
                         void (Receiver::*failed)(const dirtoo::fs::Location&, const QString&))
  {
    connect(&thumbnailer_, &thumbnail::Thumbnailer::thumbnail_ready, receiver, ready);
    connect(&thumbnailer_, &thumbnail::Thumbnailer::thumbnail_failed, receiver, failed);
    if (dir_thumb_worker_ != nullptr) {
      connect(dir_thumb_worker_, &DirectoryThumbnailWorker::thumbnail_ready, receiver, ready);
      connect(dir_thumb_worker_, &DirectoryThumbnailWorker::thumbnail_failed, receiver, failed);
    }
  }

  void shutdown();

private:
  thumbnail::Thumbnailer thumbnailer_;
  QHash<QString, QString> thumb_alias_;
  QThread* dir_thumb_thread_ = nullptr;
  DirectoryThumbnailWorker* dir_thumb_worker_ = nullptr;
  /// Absolute paths already re-queued with MatchContent for this session.
  QSet<QString> content_mime_retried_;
};

} // namespace dirtoo::app

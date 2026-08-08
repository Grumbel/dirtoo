// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/thumbnail/thumbnailer.hpp"
#include "dirtoo/fs/location.hpp"
#include "directory_thumbnail_worker.hpp"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>

#include <vector>

namespace dirtoo::app {

/// Owns D-Bus thumbnailer, archive path aliases, and directory-montage worker.
/// Viewport batching / archive extract orchestration stays on MainWindow (needs
/// collection + views); this type is the ownership boundary for R2.
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
  void clear_aliases();
  void request_many(const std::vector<dirtoo::fs::Location>& locs, const QStringList& mimes);

  /// Create directory-montage worker thread (idempotent).
  void setup_dir_worker();

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
};

} // namespace dirtoo::app

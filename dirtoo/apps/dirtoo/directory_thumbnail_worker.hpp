// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

namespace dirtoo::app {

/// Builds simple multi-tile directory montage thumbnails off the GUI thread.
/// Writes into the XDG large thumbnail cache path used by Thumbnailer.
class DirectoryThumbnailWorker : public QObject {
  Q_OBJECT

public:
  explicit DirectoryThumbnailWorker(QObject* parent = nullptr);

public slots:
  void generate(const QStringList& directory_paths);

signals:
  void thumbnail_ready(const dirtoo::fs::Location& location, const QString& path);
  /// Emitted when a single directory montage fails; message explains why.
  void thumbnail_failed(const dirtoo::fs::Location& location, const QString& message);
  void finished(int ok_count, int fail_count);
};

} // namespace dirtoo::app

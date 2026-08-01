// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/fs/location.hpp"

#include <QObject>
#include <QString>

#include <atomic>
#include <vector>

namespace dirtoo::app {

/// Lists a directory off the GUI thread.
class DirectoryLoadWorker : public QObject {
  Q_OBJECT

public:
  explicit DirectoryLoadWorker(QObject* parent = nullptr);

public slots:
  /// @param path Absolute filesystem path to list (already-resolved archive extract tree OK).
  void load(const QString& path, quint64 generation);
  /// Bump cancel token so an in-flight load abandons the result (generation still checked by UI).
  void cancel();

signals:
  void loaded(quint64 generation, std::vector<dirtoo::fs::FileInfo> items);
  void failed(quint64 generation, QString error);

private:
  std::atomic<quint64> cancel_generation_{0};
};

} // namespace dirtoo::app

Q_DECLARE_METATYPE(std::vector<dirtoo::fs::FileInfo>)

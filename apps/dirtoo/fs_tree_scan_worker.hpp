// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/tree/fs_tree_cache.hpp"
#include "dirtoo/tree/scan_tree.hpp"

#include <QObject>
#include <QString>

#include <atomic>
#include <cstdint>

namespace dirtoo::app {

/// Process-wide in-memory FsTreeCache (shared across windows).
[[nodiscard]] dirtoo::tree::FsTreeCache& app_fs_tree_cache();

/// Runs `FsTreeCache::scan` off the GUI thread.
class FsTreeScanWorker : public QObject {
  Q_OBJECT

public:
  explicit FsTreeScanWorker(QObject* parent = nullptr);

public slots:
  /// Scan @p path (UTF-8 filesystem path). @p generation identifies this request.
  void scan(const QString& path, quint64 generation, bool include_hidden, bool cross_device,
            int max_depth /* -1 = unlimited */);

  /// Abandon the in-flight scan (generation no longer matches).
  void cancel();

signals:
  void progress(quint64 generation, quint64 nodes_seen, QString current_path);
  /// Progressive sizes published; UI may refresh Contents column (throttled).
  void partial(quint64 generation, quint64 nodes_ready);
  /// Scan finished; snapshot is in `app_fs_tree_cache()` under @p path_key.
  void finished(quint64 generation, QString path_key, quint64 total_size, int state);
  void failed(quint64 generation, QString error);

private:
  std::atomic<quint64> cancel_generation_{0};
};

} // namespace dirtoo::app

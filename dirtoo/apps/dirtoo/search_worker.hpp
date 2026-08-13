// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

#include <atomic>

namespace dirtoo::app {

/// Runs recursive filter search on a background thread.
class SearchWorker : public QObject {
  Q_OBJECT

public:
  explicit SearchWorker(QObject* parent = nullptr);

public slots:
  void start(const QString& root_path, const QString& expression, bool show_hidden,
             int max_depth);
  void cancel();

signals:
  /// Absolute filesystem path of a match.
  void match_found(const QString& path, bool is_directory, quint64 size, qint64 mtime_sec);
  void finished(quint64 matched, quint64 visited, const QString& error);
  void progress(quint64 visited, quint64 matched);

private:
  std::atomic<bool> cancel_{false};
};

} // namespace dirtoo::app

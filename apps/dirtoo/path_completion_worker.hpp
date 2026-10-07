// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <cstdint>

namespace dirtoo::app {

/// Background directory scanner for location-bar path completion.
/// Only directory candidates are returned (same as the Python PathCompletionWorker).
class PathCompletionWorker : public QObject {
  Q_OBJECT

public:
  explicit PathCompletionWorker(QObject* parent = nullptr);

  /// Tell the worker that request @p latest_id is the newest one. Callable
  /// from any thread, immediately (not queued behind a running scan): a scan
  /// for an older id notices and stops instead of finishing first.
  void supersede(quint64 latest_id);

public slots:
  /// `request_id` lets the UI ignore stale results.
  void complete(quint64 request_id, const QString& text);
  /// Abort everything (shutdown). Sticky: later requests return immediately.
  void cancel();

signals:
  void completions_ready(quint64 request_id, const QString& longest_prefix,
                         const QStringList& candidates);

private:
  std::atomic<bool> cancel_{false};
  std::atomic<quint64> latest_id_{0};
};

} // namespace dirtoo::app

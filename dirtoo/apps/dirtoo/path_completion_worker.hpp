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

public slots:
  /// `request_id` lets the UI ignore stale results.
  void complete(quint64 request_id, const QString& text);
  void cancel();

signals:
  void completions_ready(quint64 request_id, const QString& longest_prefix,
                         const QStringList& candidates);

private:
  std::atomic<bool> cancel_{false};
  std::atomic<quint64> active_id_{0};
};

} // namespace dirtoo::app

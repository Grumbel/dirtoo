// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

class QThread;

namespace dirtoo::app {

class SearchWorker;

/// Owns SearchWorker + QThread lifecycle. MainWindow connects to signals for UI.
class SearchController : public QObject {
  Q_OBJECT

public:
  explicit SearchController(QObject* parent = nullptr);
  ~SearchController() override;

  /// Cancel any in-flight search and tear down the worker thread.
  void stop();

  /// Start a new search (stops any previous one first).
  void start(const QString& root_path, const QString& expression, bool show_hidden,
             int max_depth = -1);

  [[nodiscard]] bool is_running() const noexcept { return running_; }

signals:
  void match_found(const QString& path, bool is_directory, quint64 size);
  void progress(quint64 visited, quint64 matched);
  void finished(quint64 matched, quint64 visited, const QString& error);

private:
  void cleanup_thread();

  QThread* thread_ = nullptr;
  SearchWorker* worker_ = nullptr;
  bool running_ = false;
};

} // namespace dirtoo::app

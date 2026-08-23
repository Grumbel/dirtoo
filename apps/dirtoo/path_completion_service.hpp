// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "path_completion_worker.hpp"

#include <QCompleter>
#include <QLineEdit>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QStringListModel>
#include <QThread>
#include <QTimer>

namespace dirtoo::app {

/// Owns path-completion worker thread, completer model, and debounce timer.
class PathCompletionService : public QObject {
  Q_OBJECT
public:
  explicit PathCompletionService(QObject* parent = nullptr);
  ~PathCompletionService() override;

  /// Attach completer to the location line edit and start the worker thread.
  void setup(QLineEdit* edit);

  void shutdown();

  void on_text_edited(const QString& text);
  void on_timeout();
  void on_completions_ready(quint64 request_id, const QString& longest,
                            const QStringList& candidates);

  [[nodiscard]] QCompleter* completer() const { return completer_; }
  [[nodiscard]] QStringListModel* model() const { return model_; }
  [[nodiscard]] QTimer* timer() const { return timer_; }
  [[nodiscard]] PathCompletionWorker* worker() const { return worker_; }

signals:
  void completions_ready(quint64 request_id, const QString& longest_prefix,
                         const QStringList& candidates);

private:
  QLineEdit* edit_ = nullptr;
  QThread* thread_ = nullptr;
  PathCompletionWorker* worker_ = nullptr;
  QStringListModel* model_ = nullptr;
  QCompleter* completer_ = nullptr;
  QTimer* timer_ = nullptr;
  QString pending_;
  quint64 request_id_ = 0;
};

} // namespace dirtoo::app

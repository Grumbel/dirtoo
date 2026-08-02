// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "clipboard.hpp"
#include "transfer_worker.hpp"

#include <QObject>
#include <QString>
#include <QThread>

class QWidget;

namespace dirtoo::app {

class TransferDialog;

/// Owns the long-lived TransferWorker thread and starts TransferDialog sessions.
/// UI reactions stay on MainWindow via forwarded signals.
class TransferController : public QObject {
  Q_OBJECT

public:
  explicit TransferController(QObject* parent = nullptr);
  ~TransferController() override;

  /// Create dialog, wire pause/cancel, and run `request` on the worker thread.
  /// `dialog_parent` is the widget parent for TransferDialog (usually MainWindow).
  void start(QWidget* dialog_parent, const TransferRequest& request);

  [[nodiscard]] bool busy() const noexcept { return busy_; }
  [[nodiscard]] ClipboardMode last_mode() const noexcept { return last_mode_; }

  /// Shutdown worker (call from MainWindow destructor before other teardown).
  void shutdown();

  TransferWorker* worker() const noexcept { return worker_; }
  TransferDialog* dialog() const noexcept { return dialog_; }

signals:
  void item_started(int index, int total, const QString& path);
  void byte_progress(quint64 done, quint64 total, const QString& path);
  void conflict_required(const QString& destination_name, const QString& source_path,
                         const QString& destination_path);
  void finished(TransferSummary summary);
  void log_line(const QString& line);

public slots:
  void resolve_conflict(dirops::ConflictPolicy policy, bool apply, bool apply_to_all);

private:
  QThread thread_;
  TransferWorker* worker_ = nullptr;
  TransferDialog* dialog_ = nullptr;
  bool busy_ = false;
  ClipboardMode last_mode_ = ClipboardMode::Copy;
};

} // namespace dirtoo::app

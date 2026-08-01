// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "clipboard.hpp"
#include "dirops/ops.hpp"

#include <QObject>
#include <QString>

#include <filesystem>
#include <vector>

namespace dirtoo::app {

struct TransferRequest {
  ClipboardMode mode = ClipboardMode::Copy;
  std::filesystem::path destination_directory;
  std::vector<std::filesystem::path> sources;
};

struct TransferSummary {
  int completed = 0;
  int skipped = 0;
  bool cancelled = false;
  QString error; // empty if ok
};

/// Runs copy/move on a worker thread. Conflict resolution is requested via
/// signal and answered through resolve_conflict() from the UI thread.
class TransferWorker : public QObject {
  Q_OBJECT

public:
  explicit TransferWorker(QObject* parent = nullptr);

public slots:
  void run(const TransferRequest& request);
  void cancel();
  void resolve_conflict(dirops::ConflictPolicy policy, bool accepted);

signals:
  void item_started(int index, int total, const QString& path);
  void byte_progress(quint64 done, quint64 total, const QString& path);
  void conflict_required(const QString& destination_name);
  void finished(const TransferSummary& summary);

private:
  [[nodiscard]] dirops::ConflictPolicy wait_for_conflict_policy(const QString& dest_name,
                                                                bool* cancelled_out);

  std::atomic<bool> cancel_requested_{false};

  std::mutex conflict_mutex_;
  std::condition_variable conflict_cv_;
  bool conflict_pending_ = false;
  bool conflict_accepted_ = false;
  dirops::ConflictPolicy conflict_answer_ = dirops::ConflictPolicy::Fail;
};

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "clipboard.hpp"
#include "dirops/ops.hpp"

#include <QObject>
#include <QString>

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
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
  QString error;
};

/// Runs copy/move on a worker thread. Conflict resolution is requested via
/// signal and answered through resolve_conflict() from the UI thread.
class TransferWorker : public QObject {
  Q_OBJECT

public:
  explicit TransferWorker(QObject* parent = nullptr);

public slots:
  void run(TransferRequest request);
  void cancel();
  void pause();
  void resume();
  void resolve_conflict(dirops::ConflictPolicy policy, bool accepted, bool apply_to_all = false);

signals:
  void item_started(int index, int total, const QString& path);
  void byte_progress(quint64 done, quint64 total, const QString& path);
  void conflict_required(const QString& destination_name, const QString& source_path,
                         const QString& destination_path);
  void log_line(const QString& line);
  void finished(TransferSummary summary);

private:
  [[nodiscard]] dirops::ConflictPolicy wait_for_conflict_policy(
      const QString& dest_name, const std::filesystem::path& source,
      const std::filesystem::path& destination, bool* cancelled_out);
  void wait_while_paused();

  std::atomic<bool> cancel_requested_{false};
  std::atomic<bool> pause_requested_{false};

  std::mutex conflict_mutex_;
  std::condition_variable conflict_cv_;
  bool conflict_pending_ = false;
  bool conflict_accepted_ = false;
  dirops::ConflictPolicy conflict_answer_ = dirops::ConflictPolicy::Fail;
  bool conflict_have_sticky_ = false;
  dirops::ConflictPolicy conflict_sticky_ = dirops::ConflictPolicy::Fail;

  std::mutex pause_mutex_;
  std::condition_variable pause_cv_;
};

} // namespace dirtoo::app

Q_DECLARE_METATYPE(dirtoo::app::TransferRequest)
Q_DECLARE_METATYPE(dirtoo::app::TransferSummary)

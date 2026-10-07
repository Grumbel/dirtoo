// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "clipboard.hpp"
#include "conflict_probe.hpp"
#include "dirops/ops.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

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

struct TransferItemResult {
  std::filesystem::path source;
  std::filesystem::path destination;
  bool skipped = false;
};

/// What to do after an item failed (asked of the user, answered via
/// TransferWorker::resolve_error()).
enum class TransferErrorAction {
  Retry,    ///< try the same item again
  Skip,     ///< leave it, continue with the next item
  SkipAll,  ///< skip this and every later failing item without asking again
  Abort,    ///< stop the whole transfer
};

struct TransferSummary {
  int completed = 0;
  int skipped = 0;
  int failed = 0;          ///< items skipped after an error
  QStringList failures;    ///< "name: reason" for each failed item
  bool cancelled = false;
  QString error;           ///< set when the transfer was aborted by an error
  ClipboardMode mode = ClipboardMode::Copy;
  std::filesystem::path destination_directory;
  std::vector<std::filesystem::path> sources;
  std::vector<TransferItemResult> items;
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
  /// Answer to item_failed(). Like resolve_conflict(), call it directly (not
  /// queued): the worker thread is blocked waiting for it.
  void resolve_error(dirtoo::app::TransferErrorAction action);

signals:
  void item_started(int index, int total, const QString& path);
  void byte_progress(quint64 done, quint64 total, const QString& path);
  /// @p probe: source/destination metadata stat'ed on the worker thread.
  void conflict_required(const QString& destination_name, const QString& source_path,
                         const QString& destination_path, dirtoo::app::ConflictProbe probe);
  /// An item failed. @p remaining is the number of items still to do after it.
  /// The worker waits for resolve_error().
  void item_failed(const QString& source_path, const QString& message, int remaining);
  void log_line(const QString& line);
  void finished(TransferSummary summary);

private:
  [[nodiscard]] dirops::ConflictPolicy wait_for_conflict_policy(
      const QString& dest_name, const std::filesystem::path& source,
      const std::filesystem::path& destination, bool* cancelled_out);
  [[nodiscard]] TransferErrorAction wait_for_error_action(const QString& source,
                                                          const QString& message, int remaining);
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

  std::mutex error_mutex_;
  std::condition_variable error_cv_;
  bool error_pending_ = false;
  TransferErrorAction error_answer_ = TransferErrorAction::Abort;
  bool error_skip_all_ = false;

  std::mutex pause_mutex_;
  std::condition_variable pause_cv_;
};

} // namespace dirtoo::app

Q_DECLARE_METATYPE(dirtoo::app::TransferRequest)
Q_DECLARE_METATYPE(dirtoo::app::TransferSummary)
Q_DECLARE_METATYPE(dirtoo::app::TransferErrorAction)

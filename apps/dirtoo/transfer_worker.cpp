// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "transfer_worker.hpp"

#include <QMetaType>

namespace dirtoo::app {

TransferWorker::TransferWorker(QObject* parent)
    : QObject(parent)
{
  qRegisterMetaType<TransferSummary>("dirtoo::app::TransferSummary");
  qRegisterMetaType<TransferRequest>("dirtoo::app::TransferRequest");
  qRegisterMetaType<dirops::ConflictPolicy>("dirops::ConflictPolicy");
  qRegisterMetaType<ConflictProbe>("dirtoo::app::ConflictProbe");
}

void TransferWorker::cancel()
{
  {
    // Under pause_mutex_ so a waiter between its predicate check and wait()
    // cannot miss the notification below.
    std::lock_guard lock(pause_mutex_);
    cancel_requested_.store(true);
    pause_requested_.store(false);
  }
  {
    std::lock_guard lock(conflict_mutex_);
    if (conflict_pending_) {
      conflict_accepted_ = false;
      conflict_pending_ = false;
    }
  }
  conflict_cv_.notify_all();
  pause_cv_.notify_all();
}

void TransferWorker::pause()
{
  pause_requested_.store(true);
  emit log_line(QStringLiteral("Paused"));
}

void TransferWorker::resume()
{
  {
    std::lock_guard lock(pause_mutex_);
    pause_requested_.store(false);
  }
  pause_cv_.notify_all();
  emit log_line(QStringLiteral("Resumed"));
}

void TransferWorker::wait_while_paused()
{
  if (!pause_requested_.load()) {
    return;
  }
  std::unique_lock lock(pause_mutex_);
  pause_cv_.wait(lock, [this] {
    return !pause_requested_.load() || cancel_requested_.load();
  });
}

void TransferWorker::resolve_conflict(dirops::ConflictPolicy policy, bool accepted, bool apply_to_all)
{
  {
    std::lock_guard lock(conflict_mutex_);
    conflict_answer_ = policy;
    conflict_accepted_ = accepted;
    conflict_pending_ = false;
    if (accepted && apply_to_all) {
      conflict_have_sticky_ = true;
      conflict_sticky_ = policy;
    }
  }
  conflict_cv_.notify_all();
}

dirops::ConflictPolicy TransferWorker::wait_for_conflict_policy(
    const QString& dest_name, const std::filesystem::path& source,
    const std::filesystem::path& destination, bool* cancelled_out)
{
  {
    std::lock_guard lock(conflict_mutex_);
    if (conflict_have_sticky_) {
      if (cancelled_out) {
        *cancelled_out = false;
      }
      return conflict_sticky_;
    }
    conflict_pending_ = true;
    conflict_accepted_ = false;
  }
  // Stat here (worker thread) so the dialog does no I/O on the GUI thread.
  emit conflict_required(dest_name, QString::fromStdString(source.string()),
                         QString::fromStdString(destination.string()),
                         probe_conflict(source, destination));

  std::unique_lock lock(conflict_mutex_);
  conflict_cv_.wait(lock, [this] {
    return !conflict_pending_ || cancel_requested_.load();
  });

  if (cancel_requested_.load() || !conflict_accepted_) {
    if (cancelled_out) {
      *cancelled_out = true;
    }
    return dirops::ConflictPolicy::Skip;
  }
  if (cancelled_out) {
    *cancelled_out = false;
  }
  return conflict_answer_;
}

void TransferWorker::run(TransferRequest request)
{
  cancel_requested_.store(false);
  pause_requested_.store(false);
  {
    std::lock_guard lock(conflict_mutex_);
    conflict_have_sticky_ = false;
  }
  TransferSummary summary;
  summary.mode = request.mode;
  summary.destination_directory = request.destination_directory;
  summary.sources = request.sources;

  emit log_line(request.mode == ClipboardMode::Cut ? QStringLiteral("Starting move…")
                                                   : QStringLiteral("Starting copy…"));

  {
    std::error_code dir_ec;
    if (!std::filesystem::is_directory(request.destination_directory, dir_ec)) {
      // dirops would otherwise treat the missing directory as the target
      // file name and copy the first source onto that path.
      summary.error = QStringLiteral("Destination is not a directory: %1")
                          .arg(QString::fromStdString(request.destination_directory.string()));
      emit log_line(QStringLiteral("Error: %1").arg(summary.error));
      emit finished(summary);
      return;
    }
  }

  const int total = static_cast<int>(request.sources.size());
  for (int i = 0; i < total; ++i) {
    wait_while_paused();
    if (cancel_requested_.load()) {
      summary.cancelled = true;
      break;
    }

    const auto& src = request.sources[static_cast<std::size_t>(i)];
    emit item_started(i + 1, total, QString::fromStdString(src.string()));
    emit log_line(QStringLiteral("[%1/%2] %3")
                      .arg(i + 1)
                      .arg(total)
                      .arg(QString::fromStdString(src.string())));

    const auto dest = request.destination_directory / src.filename();

    dirops::Options opt;
    opt.is_cancelled = [this] {
      wait_while_paused();
      return cancel_requested_.load();
    };
    opt.on_progress = [this](std::uint64_t done, std::uint64_t tot,
                             const std::filesystem::path& p) {
      emit byte_progress(static_cast<quint64>(done), static_cast<quint64>(tot),
                         QString::fromStdString(p.string()));
    };

    // Moving into the folder the item already lives in changes nothing.
    std::error_code same_ec;
    const bool move_in_place =
        request.mode == ClipboardMode::Cut
        && std::filesystem::equivalent(src.parent_path(), request.destination_directory, same_ec)
        && !same_ec;

    // symlink_status: a dangling symlink at the destination is still a clash.
    std::error_code dest_ec;
    if (!move_in_place
        && std::filesystem::exists(std::filesystem::symlink_status(dest, dest_ec))) {
      bool user_cancelled = false;
      opt.conflict = wait_for_conflict_policy(
          QString::fromStdString(dest.filename().string()), src, dest, &user_cancelled);
      if (user_cancelled || cancel_requested_.load()) {
        summary.cancelled = true;
        emit log_line(QStringLiteral("Cancelled during conflict resolution"));
        break;
      }
      emit log_line(QStringLiteral("Conflict on %1 → policy applied")
                        .arg(QString::fromStdString(dest.filename().string())));
    }

    dirops::OpResult result;
    if (request.mode == ClipboardMode::Cut) {
      result = dirops::move_path(src, request.destination_directory, opt);
    } else {
      result = dirops::copy_path(src, request.destination_directory, opt);
    }

    if (!result) {
      summary.error = QString::fromStdString(result.error().to_string());
      emit log_line(QStringLiteral("Error: %1").arg(summary.error));
      break;
    }
    if (result->cancelled) {
      summary.cancelled = true;
      emit log_line(QStringLiteral("Cancelled"));
      break;
    }
    if (!result->items.empty()) {
      for (const auto& it : result->items) {
        TransferItemResult tir;
        tir.source = it.source;
        tir.destination = it.destination;
        tir.skipped = it.skipped;
        summary.items.push_back(std::move(tir));
        if (it.skipped) {
          ++summary.skipped;
        } else {
          ++summary.completed;
        }
      }
      if (result->items.front().skipped) {
        emit log_line(QStringLiteral("Skipped %1").arg(QString::fromStdString(src.filename().string())));
      } else {
        emit log_line(QStringLiteral("Done %1").arg(QString::fromStdString(src.filename().string())));
      }
    } else {
      TransferItemResult tir;
      tir.source = src;
      tir.destination = dest;
      tir.skipped = false;
      summary.items.push_back(std::move(tir));
      ++summary.completed;
      emit log_line(QStringLiteral("Done %1").arg(QString::fromStdString(src.filename().string())));
    }
  }

  if (!summary.cancelled && summary.error.isEmpty()) {
    emit log_line(QStringLiteral("Finished: %1 done, %2 skipped")
                      .arg(summary.completed)
                      .arg(summary.skipped));
  }
  emit finished(summary);
}

} // namespace dirtoo::app

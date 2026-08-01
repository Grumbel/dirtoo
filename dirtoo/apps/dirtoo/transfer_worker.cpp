// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "transfer_worker.hpp"

#include <QMetaType>

#include <condition_variable>
#include <mutex>

namespace dirtoo::app {

TransferWorker::TransferWorker(QObject* parent)
    : QObject(parent)
{
  qRegisterMetaType<TransferSummary>("dirtoo::app::TransferSummary");
  qRegisterMetaType<TransferRequest>("dirtoo::app::TransferRequest");
  qRegisterMetaType<dirops::ConflictPolicy>("dirops::ConflictPolicy");
}

void TransferWorker::cancel()
{
  cancel_requested_.store(true);
  {
    std::lock_guard lock(conflict_mutex_);
    if (conflict_pending_) {
      conflict_accepted_ = false;
      conflict_pending_ = false;
    }
  }
  conflict_cv_.notify_all();
}

void TransferWorker::resolve_conflict(dirops::ConflictPolicy policy, bool accepted)
{
  {
    std::lock_guard lock(conflict_mutex_);
    conflict_answer_ = policy;
    conflict_accepted_ = accepted;
    conflict_pending_ = false;
  }
  conflict_cv_.notify_all();
}

dirops::ConflictPolicy TransferWorker::wait_for_conflict_policy(const QString& dest_name,
                                                               bool* cancelled_out)
{
  {
    std::lock_guard lock(conflict_mutex_);
    conflict_pending_ = true;
    conflict_accepted_ = false;
  }
  emit conflict_required(dest_name);

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
  TransferSummary summary;

  const int total = static_cast<int>(request.sources.size());
  for (int i = 0; i < total; ++i) {
    if (cancel_requested_.load()) {
      summary.cancelled = true;
      break;
    }

    const auto& src = request.sources[static_cast<std::size_t>(i)];
    emit item_started(i + 1, total, QString::fromStdString(src.string()));

    const auto dest = request.destination_directory / src.filename();

    dirops::Options opt;
    opt.is_cancelled = [this] { return cancel_requested_.load(); };
    opt.on_progress = [this](std::uint64_t done, std::uint64_t tot,
                             const std::filesystem::path& p) {
      emit byte_progress(static_cast<quint64>(done), static_cast<quint64>(tot),
                         QString::fromStdString(p.string()));
    };

    if (std::filesystem::exists(dest)) {
      bool user_cancelled = false;
      opt.conflict = wait_for_conflict_policy(
          QString::fromStdString(dest.filename().string()), &user_cancelled);
      if (user_cancelled || cancel_requested_.load()) {
        summary.cancelled = true;
        break;
      }
    }

    dirops::OpResult result;
    if (request.mode == ClipboardMode::Cut) {
      result = dirops::move_path(src, request.destination_directory, opt);
    } else {
      result = dirops::copy_path(src, request.destination_directory, opt);
    }

    if (!result) {
      summary.error = QString::fromStdString(result.error().to_string());
      break;
    }
    if (result->cancelled) {
      summary.cancelled = true;
      break;
    }
    if (!result->items.empty() && result->items.front().skipped) {
      ++summary.skipped;
    } else {
      ++summary.completed;
    }
  }

  emit finished(summary);
}

} // namespace dirtoo::app

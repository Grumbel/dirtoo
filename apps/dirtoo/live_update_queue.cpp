// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "live_update_queue.hpp"

#include <QTimer>

namespace dirtoo::app {

LiveUpdateQueue::LiveUpdateQueue(QObject* parent)
    : QObject(parent)
    , timer_(new QTimer(this))
{
  timer_->setSingleShot(true);
  connect(timer_, &QTimer::timeout, this, [this] {
    if (!paused_ && (!pending_.isEmpty() || rescan_)) {
      emit ready();
    }
  });
}

void LiveUpdateQueue::add(const QStringList& created, const QStringList& removed,
                          const QStringList& modified)
{
  if (created.isEmpty() && removed.isEmpty() && modified.isEmpty()) {
    return;
  }
  if (paused_) {
    missed_ += static_cast<int>(created.size() + removed.size() + modified.size());
    emit missed_changed(missed_);
    return;
  }
  const auto note = [this](const QString& path, Kind kind) {
    auto it = pending_.find(path);
    if (it == pending_.end()) {
      pending_.insert(path, kind);
      order_.push_back(path);
    } else {
      it.value() = kind;
    }
  };
  // Removals first: within one burst a path both removed and created is
  // ambiguous, and an upsert is always safe (the stat decides).
  for (const QString& path : removed) {
    note(path, Kind::Removed);
  }
  for (const QString& path : created) {
    note(path, Kind::Upsert);
  }
  for (const QString& path : modified) {
    note(path, Kind::Upsert);
  }
  schedule(!created.isEmpty() || !removed.isEmpty());
}

void LiveUpdateQueue::add_rescan()
{
  if (paused_) {
    ++missed_;
    emit missed_changed(missed_);
    return;
  }
  rescan_ = true;
  schedule(true);
}

void LiveUpdateQueue::schedule(bool structural)
{
  const auto interval = structural ? structural_interval_ : modify_interval_;
  if (!timer_->isActive() || timer_->remainingTimeAsDuration() > interval) {
    timer_->start(interval);
  }
}

void LiveUpdateQueue::set_paused(bool paused)
{
  if (paused_ == paused) {
    return;
  }
  paused_ = paused;
  if (paused_) {
    // Already queued changes count as missed: they are not applied now.
    const int queued = static_cast<int>(pending_.size()) + (rescan_ ? 1 : 0);
    clear();
    if (queued > 0) {
      missed_ += queued;
      emit missed_changed(missed_);
    }
  }
}

void LiveUpdateQueue::reset_missed()
{
  if (missed_ != 0) {
    missed_ = 0;
    emit missed_changed(0);
  }
}

void LiveUpdateQueue::add_missed(int count)
{
  if (count > 0) {
    missed_ += count;
    emit missed_changed(missed_);
  }
}

void LiveUpdateQueue::clear()
{
  timer_->stop();
  pending_.clear();
  order_.clear();
  rescan_ = false;
}

LiveUpdateQueue::Batch LiveUpdateQueue::take()
{
  Batch batch;
  batch.rescan = rescan_;
  for (const QString& path : order_) {
    (pending_.value(path) == Kind::Upsert ? batch.upserts : batch.removed).push_back(path);
  }
  timer_->stop();
  pending_.clear();
  order_.clear();
  rescan_ = false;
  return batch;
}

void LiveUpdateQueue::set_intervals(std::chrono::milliseconds structural,
                                    std::chrono::milliseconds modify)
{
  structural_interval_ = structural;
  modify_interval_ = modify;
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>

#include <chrono>

class QTimer;

namespace dirtoo::app {

/// Buffers directory-watcher deltas between GUI updates.
///
/// A file that is written continuously (download, log, encoder output) raises
/// a modify event per write. Modify-only changes are therefore delivered at
/// most once per `modify_interval` (the row updates in place, so a slow tick
/// is enough to show it grow), while creations and removals still arrive after
/// `structural_interval`. Both are throttles, not debounces: steady activity
/// cannot starve updates.
///
/// While paused nothing is delivered; the queue only counts the notifications
/// it dropped so the UI can tell the user the listing is out of date.
class LiveUpdateQueue : public QObject {
  Q_OBJECT

public:
  struct Batch {
    /// Paths to stat and insert or update (created or modified). A path that
    /// turns out to be gone must be treated as removed: the watcher lists do
    /// not preserve the order of create/delete within one burst.
    QStringList upserts;
    QStringList removed;
    /// A change without names (fallback watcher, overflow): rescan.
    bool rescan = false;

    [[nodiscard]] bool empty() const { return upserts.isEmpty() && removed.isEmpty() && !rescan; }
  };

  explicit LiveUpdateQueue(QObject* parent = nullptr);

  void add(const QStringList& created, const QStringList& removed, const QStringList& modified);
  void add_rescan();

  void set_paused(bool paused);
  [[nodiscard]] bool paused() const noexcept { return paused_; }
  /// Change notifications dropped while paused (0 = the listing is current).
  [[nodiscard]] int missed() const noexcept { return missed_; }
  void reset_missed();
  /// Count changes that arrived too late to apply before pausing.
  void add_missed(int count);

  /// Drop queued changes (a full reload or navigation supersedes them).
  void clear();
  /// Remove and return everything queued.
  [[nodiscard]] Batch take();

  void set_intervals(std::chrono::milliseconds structural, std::chrono::milliseconds modify);

signals:
  /// Changes are due: call take().
  void ready();
  /// missed() changed.
  void missed_changed(int missed);

private:
  enum class Kind { Upsert, Removed };
  void schedule(bool structural);

  QTimer* timer_ = nullptr;
  std::chrono::milliseconds structural_interval_{100};
  std::chrono::milliseconds modify_interval_{1000};
  QHash<QString, Kind> pending_;
  QStringList order_;  ///< first-seen order of pending_ keys
  bool rescan_ = false;
  bool paused_ = false;
  int missed_ = 0;
};

} // namespace dirtoo::app

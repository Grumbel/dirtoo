// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <QtGlobal>

#include <atomic>

namespace dirtoo::app {

/// One named background activity (thumbs, listing, transfer, …).
struct ActivityTask {
  QString id;
  QString kind; ///< Short category ("tag", "checksum", "dir-load", …); may equal id for slots.
  QString label;
  int done = -1;  ///< <0 → indeterminate
  int total = -1;
  [[nodiscard]] QString summary() const;
  [[nodiscard]] bool active() const { return !label.isEmpty(); }
};

/// Process-wide background activity + ring buffer of recent log lines.
/// Fed from MainWindow status, workers, and the Qt message handler.
///
/// Prefer begin_job()/end_job() for concurrent work (tag, checksum, …) so each
/// run gets a unique id and the busy badge can show several tasks at once.
/// Fixed-slot set_task("dir-load", …) remains for single-flight UI slots that
/// intentionally replace the previous line of the same kind.
class ActivityMonitor : public QObject {
  Q_OBJECT
public:
  static ActivityMonitor& instance();

  /// Upsert a task by stable id (replaces label/progress when id matches).
  void set_task(const QString& id, const QString& label, int done = -1, int total = -1);
  void clear_task(const QString& id);
  void clear_all_tasks();

  /// Allocate a unique job id (`kind-N`), register the task, return the id.
  /// Concurrent jobs with different ids all appear in headline()/tasks().
  [[nodiscard]] QString begin_job(const QString& kind, const QString& label, int done = -1,
                                  int total = -1);
  /// Update an existing job (same as set_task; keeps kind from registration).
  void update_job(const QString& id, const QString& label, int done = -1, int total = -1);
  void end_job(const QString& id) { clear_task(id); }

  /// Remove every task whose id equals `kind` or starts with `kind + "-"`.
  void clear_jobs_of_kind(const QString& kind);

  [[nodiscard]] QVector<ActivityTask> tasks() const;
  [[nodiscard]] bool any_active() const;
  /// Short toolbar/status text (e.g. "Thumbs 12/40 · Loading…").
  [[nodiscard]] QString headline() const;

  void append_log(QtMsgType type, const QString& message);
  [[nodiscard]] QStringList recent_logs(int max_lines = 400) const;
  void clear_logs();

signals:
  void changed();
  void log_appended(const QString& line);

private:
  explicit ActivityMonitor(QObject* parent = nullptr);

  QVector<ActivityTask> tasks_;
  QStringList log_;
  std::atomic<quint64> job_seq_{0};
  static constexpr int kMaxLog = 800;
};

/// RAII helper: end_job on destruction (move-only).
class ScopedActivityJob {
public:
  ScopedActivityJob() = default;
  ScopedActivityJob(const QString& kind, const QString& label, int done = -1, int total = -1)
      : id_(ActivityMonitor::instance().begin_job(kind, label, done, total))
  {
  }
  ~ScopedActivityJob() { release(); }

  ScopedActivityJob(const ScopedActivityJob&) = delete;
  ScopedActivityJob& operator=(const ScopedActivityJob&) = delete;

  ScopedActivityJob(ScopedActivityJob&& other) noexcept
      : id_(std::move(other.id_))
  {
    other.id_.clear();
  }
  ScopedActivityJob& operator=(ScopedActivityJob&& other) noexcept
  {
    if (this != &other) {
      release();
      id_ = std::move(other.id_);
      other.id_.clear();
    }
    return *this;
  }

  [[nodiscard]] const QString& id() const { return id_; }
  [[nodiscard]] bool active() const { return !id_.isEmpty(); }

  void update(const QString& label, int done = -1, int total = -1)
  {
    if (!id_.isEmpty()) {
      ActivityMonitor::instance().update_job(id_, label, done, total);
    }
  }

  void release()
  {
    if (!id_.isEmpty()) {
      ActivityMonitor::instance().end_job(id_);
      id_.clear();
    }
  }

private:
  QString id_;
};

} // namespace dirtoo::app

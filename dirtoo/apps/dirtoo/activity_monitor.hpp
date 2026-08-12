// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <QtGlobal>

namespace dirtoo::app {

/// One named background activity (thumbs, listing, transfer, …).
struct ActivityTask {
  QString id;
  QString label;
  int done = -1;  ///< <0 → indeterminate
  int total = -1;
  [[nodiscard]] QString summary() const;
  [[nodiscard]] bool active() const { return !label.isEmpty(); }
};

/// Process-wide background activity + ring buffer of recent log lines.
/// Fed from MainWindow status, workers, and the Qt message handler.
class ActivityMonitor : public QObject {
  Q_OBJECT
public:
  static ActivityMonitor& instance();

  void set_task(const QString& id, const QString& label, int done = -1, int total = -1);
  void clear_task(const QString& id);
  void clear_all_tasks();

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
  static constexpr int kMaxLog = 800;
};

} // namespace dirtoo::app

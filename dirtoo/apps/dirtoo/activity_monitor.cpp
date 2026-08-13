// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "activity_monitor.hpp"

#include <QMutex>
#include <QMutexLocker>

#include <algorithm>

namespace dirtoo::app {
namespace {

QMutex& monitor_mu()
{
  static QMutex m;
  return m;
}

QString level_tag(QtMsgType type)
{
  switch (type) {
  case QtDebugMsg:
    return QStringLiteral("D");
  case QtInfoMsg:
    return QStringLiteral("I");
  case QtWarningMsg:
    return QStringLiteral("W");
  case QtCriticalMsg:
    return QStringLiteral("C");
  case QtFatalMsg:
    return QStringLiteral("F");
  default:
    return QStringLiteral("?");
  }
}

} // namespace

QString ActivityTask::summary() const
{
  if (label.isEmpty()) {
    return {};
  }
  if (total > 0 && done >= 0) {
    const int pct = static_cast<int>((100.0 * std::min(done, total)) / total);
    return QStringLiteral("%1 %2/%3 (%4%)").arg(label).arg(done).arg(total).arg(pct);
  }
  if (done >= 0 && total <= 0) {
    return QStringLiteral("%1 %2").arg(label).arg(done);
  }
  return label;
}

ActivityMonitor& ActivityMonitor::instance()
{
  static ActivityMonitor mon;
  return mon;
}

ActivityMonitor::ActivityMonitor(QObject* parent)
    : QObject(parent)
{
}

void ActivityMonitor::set_task(const QString& id, const QString& label, int done, int total)
{
  if (id.isEmpty()) {
    return;
  }
  if (label.isEmpty()) {
    clear_task(id);
    return;
  }
  {
    QMutexLocker lock(&monitor_mu());
    for (auto& task : tasks_) {
      if (task.id == id) {
        task.label = label;
        task.done = done;
        task.total = total;
        goto emit_out;
      }
    }
    ActivityTask task;
    task.id = id;
    task.label = label;
    task.done = done;
    task.total = total;
    tasks_.push_back(std::move(task));
  }
emit_out:
  emit changed();
}

void ActivityMonitor::clear_task(const QString& id)
{
  bool changed_flag = false;
  {
    QMutexLocker lock(&monitor_mu());
    for (int i = 0; i < tasks_.size(); ++i) {
      if (tasks_[i].id == id) {
        tasks_.removeAt(i);
        changed_flag = true;
        break;
      }
    }
  }
  if (changed_flag) {
    emit changed();
  }
}

void ActivityMonitor::clear_all_tasks()
{
  {
    QMutexLocker lock(&monitor_mu());
    if (tasks_.isEmpty()) {
      return;
    }
    tasks_.clear();
  }
  emit changed();
}

QVector<ActivityTask> ActivityMonitor::tasks() const
{
  QMutexLocker lock(&monitor_mu());
  return tasks_;
}

bool ActivityMonitor::any_active() const
{
  QMutexLocker lock(&monitor_mu());
  return !tasks_.isEmpty();
}

QString ActivityMonitor::headline() const
{
  QMutexLocker lock(&monitor_mu());
  if (tasks_.isEmpty()) {
    return QStringLiteral("Idle");
  }
  QStringList parts;
  for (const auto& t : tasks_) {
    const QString s = t.summary();
    if (!s.isEmpty()) {
      parts << s;
    }
  }
  if (parts.isEmpty()) {
    return QStringLiteral("Working…");
  }
  // Keep toolbar text short but allow multi-task detail.
  QString out = parts.join(QStringLiteral(" · "));
  if (out.size() > 72) {
    out = out.left(69) + QStringLiteral("…");
  }
  return out;
}

void ActivityMonitor::append_log(QtMsgType type, const QString& message)
{
  const QString line =
      QStringLiteral("[%1] %2").arg(level_tag(type), message.trimmed());
  {
    QMutexLocker lock(&monitor_mu());
    log_.push_back(line);
    while (log_.size() > kMaxLog) {
      log_.removeFirst();
    }
  }
  emit log_appended(line);
}

QStringList ActivityMonitor::recent_logs(int max_lines) const
{
  QMutexLocker lock(&monitor_mu());
  if (max_lines <= 0 || log_.size() <= max_lines) {
    return log_;
  }
  return log_.mid(log_.size() - max_lines);
}

void ActivityMonitor::clear_logs()
{
  {
    QMutexLocker lock(&monitor_mu());
    log_.clear();
  }
  emit changed();
}

} // namespace dirtoo::app

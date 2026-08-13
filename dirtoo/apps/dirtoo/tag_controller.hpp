// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"
#include "tag_job.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

#include <deque>
#include <vector>

class QWidget;

namespace dirtoo::app {

/// Tag… UI: name dialog + background TagJob queue (one writer at a time).
class TagController : public QObject {
  Q_OBJECT
public:
  explicit TagController(QObject* parent = nullptr);

  void set_dialog_parent(QWidget* parent);

  /// Filter selection, prompt for tags, enqueue job (non-modal).
  void tag_files(std::vector<dirtoo::fs::FileInfo> selection);

signals:
  void status_message(const QString& text, int timeout_ms = 5000);
  void tags_applied(int tagged);

private:
  struct Pending {
    std::vector<dirtoo::fs::FileInfo> files;
    QStringList tags;
    TagJob::Mode mode = TagJob::Mode::Add;
  };

  void enqueue_job(Pending pending);
  void start_next_job();

  QWidget* dialog_parent_ = nullptr;
  std::deque<Pending> queue_;
  TagJob* active_job_ = nullptr;
  int job_seq_ = 0;
};

} // namespace dirtoo::app

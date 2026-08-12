// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

#include <vector>

class QWidget;

namespace dirtoo::app {

/// Tag… UI: name dialog, progress, TagJob lifecycle. MainWindow supplies the
/// selection and reacts to tags_applied (chip cache + view refresh).
class TagController : public QObject {
  Q_OBJECT
public:
  explicit TagController(QObject* parent = nullptr);

  /// Parent for dialogs (typically MainWindow).
  void set_dialog_parent(QWidget* parent);

  /// Filter to regular files / archive members, prompt for tag name, run job.
  void tag_files(std::vector<dirtoo::fs::FileInfo> selection);

signals:
  void status_message(const QString& text, int timeout_ms = 5000);
  /// Fired when at least one file was tagged successfully.
  void tags_applied(int tagged);

private:
  QWidget* dialog_parent_ = nullptr;
};

} // namespace dirtoo::app

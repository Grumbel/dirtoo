// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace dirtoo::app {

/// Detailed background activity + recent application log lines.
class ActivityDialog : public QDialog {
  Q_OBJECT
public:
  explicit ActivityDialog(QWidget* parent = nullptr);

public slots:
  void refresh();

private:
  QListWidget* task_list_ = nullptr;
  QLabel* empty_label_ = nullptr;
  QPlainTextEdit* log_view_ = nullptr;
};

void show_activity_dialog(QWidget* parent);

} // namespace dirtoo::app

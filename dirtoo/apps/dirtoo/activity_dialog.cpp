// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "activity_dialog.hpp"

#include "activity_monitor.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace dirtoo::app {

ActivityDialog::ActivityDialog(QWidget* parent)
    : QDialog(parent)
{
  setWindowTitle(QStringLiteral("Background activity"));
  setAttribute(Qt::WA_DeleteOnClose);
  resize(560, 420);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(QStringLiteral("Active tasks"), this));
  task_list_ = new QListWidget(this);
  task_list_->setMinimumHeight(100);
  layout->addWidget(task_list_);
  empty_label_ = new QLabel(QStringLiteral("No background tasks right now."), this);
  empty_label_->setStyleSheet(QStringLiteral("color: gray;"));
  layout->addWidget(empty_label_);

  layout->addWidget(new QLabel(QStringLiteral("Recent log messages"), this));
  log_view_ = new QPlainTextEdit(this);
  log_view_->setReadOnly(true);
  log_view_->setLineWrapMode(QPlainTextEdit::NoWrap);
  log_view_->setMaximumBlockCount(800);
  layout->addWidget(log_view_, 1);

  auto* row = new QHBoxLayout();
  auto* clear_logs = new QPushButton(QStringLiteral("Clear log"), this);
  auto* refresh_btn = new QPushButton(QStringLiteral("Refresh"), this);
  row->addWidget(clear_logs);
  row->addWidget(refresh_btn);
  row->addStretch(1);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  row->addWidget(buttons);
  layout->addLayout(row);

  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(clear_logs, &QPushButton::clicked, this, [] {
    ActivityMonitor::instance().clear_logs();
  });
  connect(refresh_btn, &QPushButton::clicked, this, &ActivityDialog::refresh);
  connect(&ActivityMonitor::instance(), &ActivityMonitor::changed, this, &ActivityDialog::refresh);
  connect(&ActivityMonitor::instance(), &ActivityMonitor::log_appended, this,
          [this](const QString& line) {
            if (log_view_ != nullptr) {
              log_view_->appendPlainText(line);
            }
          });

  refresh();
}

void ActivityDialog::refresh()
{
  const auto tasks = ActivityMonitor::instance().tasks();
  task_list_->clear();
  for (const auto& t : tasks) {
    task_list_->addItem(t.summary());
  }
  const bool empty = tasks.isEmpty();
  task_list_->setVisible(!empty);
  empty_label_->setVisible(empty);

  // Full refresh of log (e.g. after clear)
  log_view_->setPlainText(ActivityMonitor::instance().recent_logs().join(QChar('\n')));
  auto cursor = log_view_->textCursor();
  cursor.movePosition(QTextCursor::End);
  log_view_->setTextCursor(cursor);
}

void show_activity_dialog(QWidget* parent)
{
  auto* dlg = new ActivityDialog(parent);
  dlg->show();
  dlg->raise();
  dlg->activateWindow();
}

} // namespace dirtoo::app

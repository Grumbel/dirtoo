// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "transfer_dialog.hpp"

#include <QCheckBox>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace dirtoo::app {

TransferDialog::TransferDialog(QWidget* parent)
    : QDialog(parent)
{
  setWindowTitle(QStringLiteral("Transferring files"));
  setModal(true);
  setMinimumWidth(480);

  auto* layout = new QVBoxLayout(this);
  title_label_ = new QLabel(QStringLiteral("<big>File transfer in progress:</big>"), this);
  title_label_->setTextFormat(Qt::RichText);
  layout->addWidget(title_label_);

  auto* info = new QGroupBox(QStringLiteral("Info:"), this);
  auto* form = new QFormLayout(info);
  dest_label_ = new QLabel(this);
  dest_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  dest_label_->setWordWrap(true);
  file_label_ = new QLabel(this);
  file_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  file_label_->setWordWrap(true);
  item_label_ = new QLabel(this);
  transferred_label_ = new QLabel(QStringLiteral("—"), this);
  time_label_ = new QLabel(QStringLiteral("00:00:00"), this);
  bar_ = new QProgressBar(this);
  bar_->setRange(0, 100);
  bar_->setValue(0);
  form->addRow(QStringLiteral("Destination:"), dest_label_);
  form->addRow(QStringLiteral("Source:"), file_label_);
  form->addRow(QStringLiteral("Items:"), item_label_);
  form->addRow(QStringLiteral("Progress:"), bar_);
  form->addRow(QStringLiteral("Transferred:"), transferred_label_);
  form->addRow(QStringLiteral("Time elapsed:"), time_label_);
  layout->addWidget(info);

  close_when_finished_ = new QCheckBox(QStringLiteral("Close when finished"), this);
  close_when_finished_->setChecked(true);
  layout->addWidget(close_when_finished_);

  auto* buttons = new QHBoxLayout();
  buttons->addStretch(1);
  cancel_btn_ = new QPushButton(QStringLiteral("Cancel"), this);
  close_btn_ = new QPushButton(QStringLiteral("Close"), this);
  close_btn_->setVisible(false);
  buttons->addWidget(cancel_btn_);
  buttons->addWidget(close_btn_);
  layout->addLayout(buttons);

  elapsed_ = new QElapsedTimer();
  ui_timer_ = new QTimer(this);
  ui_timer_->setInterval(250);
  connect(ui_timer_, &QTimer::timeout, this, &TransferDialog::on_tick);

  connect(cancel_btn_, &QPushButton::clicked, this, &TransferDialog::on_cancel);
  connect(close_btn_, &QPushButton::clicked, this, &QDialog::accept);
}

void TransferDialog::set_title_text(const QString& text)
{
  title_label_->setText(QStringLiteral("<big>%1</big>").arg(text.toHtmlEscaped()));
}

void TransferDialog::set_destination(const QString& path)
{
  dest_label_->setText(path);
}

void TransferDialog::set_current_file(const QString& path)
{
  file_label_->setText(path);
}

void TransferDialog::set_progress(std::uint64_t done, std::uint64_t total)
{
  bytes_done_ = done;
  bytes_total_ = total;
  if (total == 0) {
    bar_->setRange(0, 0); // busy
  } else {
    bar_->setRange(0, 100);
    bar_->setValue(static_cast<int>((done * 100) / total));
  }
  update_transferred_label();
}

void TransferDialog::set_item_progress(int current_item, int total_items)
{
  item_label_->setText(QStringLiteral("%1 of %2").arg(current_item).arg(total_items));
}

void TransferDialog::mark_finished(bool cancelled, const QString& error)
{
  ui_timer_->stop();
  update_time_label();
  cancel_btn_->setEnabled(false);
  cancel_btn_->setVisible(false);
  close_btn_->setVisible(true);
  close_btn_->setDefault(true);
  if (!error.isEmpty()) {
    title_label_->setText(QStringLiteral("<big>Transfer failed</big>"));
    file_label_->setText(error);
  } else if (cancelled) {
    title_label_->setText(QStringLiteral("<big>Transfer cancelled</big>"));
  } else {
    title_label_->setText(QStringLiteral("<big>Transfer complete</big>"));
    bar_->setRange(0, 100);
    bar_->setValue(100);
  }
  if (close_when_finished() && error.isEmpty() && !cancelled) {
    accept();
  }
}

bool TransferDialog::close_when_finished() const
{
  return close_when_finished_ != nullptr && close_when_finished_->isChecked();
}

void TransferDialog::reset()
{
  cancelled_.store(false);
  bytes_done_ = 0;
  bytes_total_ = 0;
  title_label_->setText(QStringLiteral("<big>File transfer in progress:</big>"));
  dest_label_->clear();
  file_label_->clear();
  item_label_->clear();
  transferred_label_->setText(QStringLiteral("—"));
  time_label_->setText(QStringLiteral("00:00:00"));
  bar_->setRange(0, 100);
  bar_->setValue(0);
  cancel_btn_->setEnabled(true);
  cancel_btn_->setVisible(true);
  cancel_btn_->setText(QStringLiteral("Cancel"));
  close_btn_->setVisible(false);
  elapsed_->restart();
  ui_timer_->start();
}

void TransferDialog::on_cancel()
{
  cancelled_.store(true);
  cancel_btn_->setEnabled(false);
  cancel_btn_->setText(QStringLiteral("Cancelling…"));
  emit cancel_requested();
}

void TransferDialog::on_tick()
{
  update_time_label();
}

void TransferDialog::update_transferred_label()
{
  const QString done = QLocale::system().formattedDataSize(static_cast<qint64>(bytes_done_));
  if (bytes_total_ == 0) {
    transferred_label_->setText(done);
  } else {
    const QString total = QLocale::system().formattedDataSize(static_cast<qint64>(bytes_total_));
    transferred_label_->setText(QStringLiteral("%1 / %2").arg(done, total));
  }
}

void TransferDialog::update_time_label()
{
  if (elapsed_ == nullptr || !elapsed_->isValid()) {
    return;
  }
  const qint64 ms = elapsed_->elapsed();
  const int secs = static_cast<int>(ms / 1000);
  const int h = secs / 3600;
  const int m = (secs % 3600) / 60;
  const int s = secs % 60;
  time_label_->setText(QStringLiteral("%1:%2:%3")
                           .arg(h, 2, 10, QLatin1Char('0'))
                           .arg(m, 2, 10, QLatin1Char('0'))
                           .arg(s, 2, 10, QLatin1Char('0')));
}

} // namespace dirtoo::app

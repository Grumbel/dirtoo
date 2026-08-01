// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "transfer_dialog.hpp"

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace dirtoo::app {

TransferDialog::TransferDialog(QWidget* parent)
    : QDialog(parent)
{
  setWindowTitle(QStringLiteral("Transfer"));
  setModal(true);
  setMinimumWidth(420);

  auto* layout = new QVBoxLayout(this);
  title_label_ = new QLabel(QStringLiteral("Working…"), this);
  file_label_ = new QLabel(this);
  file_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  item_label_ = new QLabel(this);
  bar_ = new QProgressBar(this);
  bar_->setRange(0, 100);
  bar_->setValue(0);
  cancel_btn_ = new QPushButton(QStringLiteral("Cancel"), this);

  layout->addWidget(title_label_);
  layout->addWidget(file_label_);
  layout->addWidget(item_label_);
  layout->addWidget(bar_);
  layout->addWidget(cancel_btn_);

  connect(cancel_btn_, &QPushButton::clicked, this, &TransferDialog::on_cancel);
}

void TransferDialog::set_title_text(const QString& text)
{
  title_label_->setText(text);
}

void TransferDialog::set_current_file(const QString& path)
{
  file_label_->setText(path);
}

void TransferDialog::set_progress(std::uint64_t done, std::uint64_t total)
{
  if (total == 0) {
    bar_->setRange(0, 0); // busy indicator
    return;
  }
  bar_->setRange(0, 100);
  bar_->setValue(static_cast<int>((done * 100) / total));
}

void TransferDialog::set_item_progress(int current_item, int total_items)
{
  item_label_->setText(QStringLiteral("Item %1 of %2").arg(current_item).arg(total_items));
  if (total_items > 0) {
    bar_->setRange(0, total_items);
    bar_->setValue(current_item);
  }
}

void TransferDialog::on_cancel()
{
  cancelled_.store(true);
  cancel_btn_->setEnabled(false);
  cancel_btn_->setText(QStringLiteral("Cancelling…"));
}

} // namespace dirtoo::app

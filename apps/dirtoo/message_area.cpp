// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "message_area.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QTimer>

namespace dirtoo::app {

MessageArea::MessageArea(QWidget* parent)
    : QFrame(parent)
{
  setFrameShape(QFrame::StyledPanel);
  label_ = new QLabel(this);
  label_->setWordWrap(true);
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 4, 8, 4);
  layout->addWidget(label_);
  timer_ = new QTimer(this);
  timer_->setSingleShot(true);
  connect(timer_, &QTimer::timeout, this, &MessageArea::clear);
  hide();
}

void MessageArea::show_info(const QString& text, int timeout_ms)
{
  setStyleSheet(QStringLiteral("MessageArea { background: palette(mid); }"));
  label_->setText(text);
  show();
  timer_->start(timeout_ms);
}

void MessageArea::show_error(const QString& text, int timeout_ms)
{
  // Errors always go to stderr (default filter is QtWarningMsg); --verbose/--debug
  // remain for noisier info/debug traffic.
  qWarning().noquote() << QStringLiteral("error: %1").arg(text);
  setStyleSheet(QStringLiteral(
      "MessageArea { background: #5a2020; color: white; } QLabel { color: white; }"));
  label_->setText(text);
  show();
  timer_->start(timeout_ms);
}

void MessageArea::clear()
{
  hide();
  label_->clear();
  timer_->stop();
}

} // namespace dirtoo::app

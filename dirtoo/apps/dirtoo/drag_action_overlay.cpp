// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "drag_action_overlay.hpp"

#include <QApplication>
#include <QCursor>
#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>
#include <QTimerEvent>

#include <algorithm>

namespace dirtoo::app {

DragActionOverlay::DragActionOverlay(QWidget* parent)
    : QWidget(parent,
              Qt::Window | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint
                  | Qt::FramelessWindowHint | Qt::Tool)
{
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_ShowWithoutActivating);
  resize(56, 22);
  timer_id_ = startTimer(1000 / 60);
  show();
}

void DragActionOverlay::timerEvent(QTimerEvent* event)
{
  if (event->timerId() != timer_id_) {
    QWidget::timerEvent(event);
    return;
  }
  if (!(QApplication::mouseButtons() & Qt::LeftButton)) {
    hide();
    killTimer(timer_id_);
    timer_id_ = 0;
    deleteLater();
    return;
  }
  const QPoint pos = QCursor::pos();
  move(pos.x() + 16, pos.y() - 12);

  const Qt::KeyboardModifiers mods = QApplication::queryKeyboardModifiers();
  QString next;
  if ((mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier)) {
    next = QStringLiteral("Link");
  } else if (mods & Qt::ControlModifier) {
    next = QStringLiteral("Copy");
  } else if (mods & Qt::ShiftModifier) {
    next = QStringLiteral("Move");
  } else if (mods & Qt::AltModifier) {
    next = QStringLiteral("Link");
  }
  if (next != text_) {
    text_ = next;
    if (text_.isEmpty()) {
      hide();
    } else {
      const QFontMetrics fm(font());
      resize(std::max(48, fm.horizontalAdvance(text_) + 12), std::max(20, fm.height() + 6));
      show();
      update();
    }
  } else if (!text_.isEmpty()) {
    show();
  }
}

void DragActionOverlay::paintEvent(QPaintEvent* event)
{
  (void)event;
  if (text_.isEmpty()) {
    return;
  }
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(30, 30, 30, 200));
  p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);
  p.setPen(Qt::white);
  p.drawText(rect(), Qt::AlignCenter, text_);
}

void begin_drag_action_overlay()
{
  // Parentless top-level; self-deletes when the mouse button is released.
  new DragActionOverlay(nullptr);
}

} // namespace dirtoo::app

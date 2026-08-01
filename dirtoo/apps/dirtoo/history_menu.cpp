// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "history_menu.hpp"

#include <QMouseEvent>

namespace dirtoo::app {

HistoryMenu::HistoryMenu(const QString& title, QWidget* parent)
    : QMenu(title, parent)
{
}

void HistoryMenu::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::MiddleButton) {
    middle_pressed_ = true;
  }
  QMenu::mousePressEvent(event);
}

void HistoryMenu::mouseReleaseEvent(QMouseEvent* event)
{
  // Keep middle_pressed_ true through the action trigger that Qt fires on
  // release, then clear it after the base handler returns.
  QMenu::mouseReleaseEvent(event);
  if (event->button() == Qt::MiddleButton) {
    middle_pressed_ = false;
  }
}

} // namespace dirtoo::app

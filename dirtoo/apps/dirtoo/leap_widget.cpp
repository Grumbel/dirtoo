// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "leap_widget.hpp"

#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QShowEvent>
#include <QVBoxLayout>

namespace dirtoo::app {

LeapWidget::LeapWidget(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
  edit_ = new QLineEdit(this);
  edit_->setPlaceholderText(QStringLiteral("Jump to…"));
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->addWidget(edit_);
  resize(220, 40);

  connect(edit_, &QLineEdit::textChanged, this, [this](const QString& text) {
    emit leap(text, true, false);
  });
  connect(edit_, &QLineEdit::returnPressed, this, [this] {
    emit leap(edit_->text(), true, true);
    hide();
  });
  edit_->installEventFilter(this);
}

void LeapWidget::clear()
{
  edit_->clear();
}

QString LeapWidget::text() const
{
  return edit_->text();
}

void LeapWidget::show_and_focus()
{
  clear();
  show();
  place_on_parent();
  edit_->setFocus(Qt::PopupFocusReason);
}

void LeapWidget::place_on_parent()
{
  QWidget* p = parentWidget();
  if (p == nullptr) {
    return;
  }
  const QPoint bottom_right = p->mapToGlobal(p->rect().bottomRight());
  move(bottom_right.x() - width() - 8, bottom_right.y() - height() - 8);
}

void LeapWidget::showEvent(QShowEvent* event)
{
  QWidget::showEvent(event);
  place_on_parent();
}

void LeapWidget::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Escape) {
    hide();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}

bool LeapWidget::eventFilter(QObject* obj, QEvent* event)
{
  if (obj == edit_ && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Escape) {
      hide();
      return true;
    }
    if (ke->key() == Qt::Key_Up) {
      emit leap(edit_->text(), false, true);
      return true;
    }
    if (ke->key() == Qt::Key_Down) {
      emit leap(edit_->text(), true, true);
      return true;
    }
  }
  if (obj == edit_ && event->type() == QEvent::FocusOut) {
    // Delay hide so clicks on the list still work; simple: hide on focus out.
    hide();
  }
  return QWidget::eventFilter(obj, event);
}

} // namespace dirtoo::app

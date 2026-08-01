// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

namespace dirtoo::app {

/// Floating label next to the cursor during a drag (Copy / Move / Link), matching
/// the Python DragWidget workaround for Qt's static drag cursor limitation.
class DragActionOverlay : public QWidget {
  Q_OBJECT
public:
  explicit DragActionOverlay(QWidget* parent = nullptr);

protected:
  void timerEvent(QTimerEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

private:
  int timer_id_ = 0;
  QString text_;
};

/// Starts an overlay for the duration of a drag.exec()-style operation.
void begin_drag_action_overlay();

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMenu>

namespace dirtoo::app {

/// QMenu that tracks middle-button state so actions can open in a new window
/// (same idea as Python gui/menu.py addDoubleAction).
class HistoryMenu : public QMenu {
  Q_OBJECT

public:
  explicit HistoryMenu(const QString& title, QWidget* parent = nullptr);

  [[nodiscard]] bool middle_pressed() const { return middle_pressed_; }

protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  bool middle_pressed_ = false;
};

} // namespace dirtoo::app

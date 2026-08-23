// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFrame>

class QLabel;
class QTimer;

namespace dirtoo::app {

/// Transient banner for non-modal messages (Python MessageArea analogue).
class MessageArea : public QFrame {
  Q_OBJECT

public:
  explicit MessageArea(QWidget* parent = nullptr);

  void show_info(const QString& text, int timeout_ms = 5000);
  void show_error(const QString& text, int timeout_ms = 8000);
  void clear();

private:
  QLabel* label_ = nullptr;
  QTimer* timer_ = nullptr;
};

} // namespace dirtoo::app

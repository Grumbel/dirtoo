// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QToolButton>

namespace dirtoo::app {

/// Toolbar control: shows Idle / active headline; click opens ActivityDialog.
class ActivityIndicator : public QToolButton {
  Q_OBJECT
public:
  explicit ActivityIndicator(QWidget* parent = nullptr);

public slots:
  void refresh();

private:
  void open_dialog();
};

} // namespace dirtoo::app

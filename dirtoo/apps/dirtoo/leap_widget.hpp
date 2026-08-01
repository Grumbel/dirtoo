// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

class QLineEdit;

namespace dirtoo::app {

/// Frameless overlay for type-ahead jump in the file list (Python LeapWidget).
class LeapWidget : public QWidget {
  Q_OBJECT

public:
  explicit LeapWidget(QWidget* parent = nullptr);

  void clear();
  void place_on_parent();
  [[nodiscard]] QString text() const;

public slots:
  void show_and_focus();

signals:
  /// text, forward direction, wrap selection
  void leap(const QString& text, bool forward, bool from_key);

protected:
  void showEvent(QShowEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  bool eventFilter(QObject* obj, QEvent* event) override;

private:
  QLineEdit* edit_ = nullptr;
};

} // namespace dirtoo::app

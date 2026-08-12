// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"

#include <QString>
#include <QStringList>
#include <QWidget>

#include <vector>

class QHBoxLayout;
class QScrollArea;
class QToolButton;

namespace dirtoo::app {

/// Bottom strip of filter chips (browser bookmark-bar style).
/// Auto buttons from the current listing (type:image/video/…, tag:…);
/// user can pin the active filter expression.
class QuickFilterBar : public QWidget {
  Q_OBJECT
public:
  explicit QuickFilterBar(QWidget* parent = nullptr);

  /// Rebuild auto chips from the directory listing (not the filtered visible set).
  void rebuild_from_items(const std::vector<dirtoo::fs::FileInfo>& items);

  /// Highlight the chip whose expression matches @p expr (or none).
  void set_active_expression(const QString& expr);

  /// Add a pinned chip for @p expr (no-op if empty or already present).
  void pin_expression(const QString& expr, const QString& label = {});

  [[nodiscard]] QStringList pinned_expressions() const { return pinned_; }

signals:
  /// Empty string clears the filter.
  void filter_requested(const QString& expression);
  /// User asked to pin whatever is currently in the filter line.
  void pin_current_requested();

private:
  void rebuild_buttons();
  void clear_auto_buttons();
  QToolButton* make_chip(const QString& label, const QString& expression, bool pinned);

  QScrollArea* scroll_ = nullptr;
  QWidget* strip_ = nullptr;
  QHBoxLayout* strip_layout_ = nullptr;
  QToolButton* pin_btn_ = nullptr;

  struct Chip {
    QString label;
    QString expression;
    bool pinned = false;
  };
  std::vector<Chip> auto_chips_;
  QStringList pinned_;
  QString active_;
};

} // namespace dirtoo::app

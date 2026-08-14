// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"
#include "flow_layout.hpp"

#include <QColor>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <vector>

class QScrollArea;
class QToolButton;

namespace dirtoo::app {

/// Where a pinned QuickFilter is visible.
enum class QuickFilterScope {
  Everywhere, ///< Always show the chip
  Directory,  ///< Only when the current location equals a listed path
  Subtree,    ///< When current location is equal to or under a listed path
};

struct PinnedQuickFilter {
  QString expression;
  QString label; ///< Empty → show expression (elided)
  QuickFilterScope scope = QuickFilterScope::Subtree;
  /// Absolute directory paths. Empty + Directory/Subtree uses path at pin time
  /// via set_current_directory. Multiple paths: any match shows the chip.
  QStringList directories;
};

/// Bottom strip of filter chips (browser bookmark-bar style).
/// Auto buttons from the current listing (type:…, untagged helpers, tag:…);
/// user pins with optional label and directory scope.
class QuickFilterBar : public QWidget {
  Q_OBJECT
public:
  explicit QuickFilterBar(QWidget* parent = nullptr);

  /// Current location used for Directory/Subtree visibility (normalized path).
  void set_current_directory(const QString& path);

  /// Rebuild auto chips from the directory listing (not the filtered visible set).
  void rebuild_from_items(const std::vector<dirtoo::fs::FileInfo>& items);

  /// Highlight the chip whose expression matches @p expr (or none).
  void set_active_expression(const QString& expr);

  /// Pin @p expr (no-op if empty). Uses current_directory_ for initial scope paths.
  void pin_expression(const QString& expr, const QString& label = {});

signals:
  /// Empty string clears the filter.
  void filter_requested(const QString& expression);
  /// User asked to pin whatever is currently in the filter line.
  void pin_current_requested();
  /// Set renamed/colored/dissolved from a QuickFilter chip menu.
  void sets_changed();

private:
  void rebuild_buttons();
  void clear_buttons();
  QToolButton* make_pinned_chip(int pin_index);
  void show_pin_menu(int pin_index, const QPoint& global_pos);
  void edit_pin_expression(int pin_index);
  void edit_pin_label(int pin_index);
  void edit_pin_directories(int pin_index);
  void set_pin_scope(int pin_index, QuickFilterScope scope);
  void remove_pin(int pin_index);
  [[nodiscard]] bool pin_visible(const PinnedQuickFilter& pin) const;
  void load_pins();
  void save_pins() const;

  QScrollArea* scroll_ = nullptr;
  QWidget* strip_ = nullptr;
  FlowLayout* strip_layout_ = nullptr;
  QToolButton* pin_btn_ = nullptr;

  struct AutoChip {
    QString label;
    QString expression;
    QColor accent; ///< Invalid = default chrome; set for tags/sets
    QString set_id; ///< Non-empty for Group::Set chips (stable id for menus).
    /// Groups for separators: type → helper → tag → set (then pinned).
    enum class Group { Type, Helper, Tag, Set };
    Group group = Group::Type;
  };
  QToolButton* make_auto_chip(const AutoChip& chip);
  QWidget* make_separator();
  void show_set_menu(const QString& set_id, const QPoint& global_pos);
  std::vector<AutoChip> auto_chips_;
  std::vector<PinnedQuickFilter> pins_;
  QString active_;
  QString current_directory_;
};

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QString>

class QSettings;

namespace dirtoo::app {

/// Product chrome colors (Preferences → Colors). Defaults match the shipped look.
struct UiColors {
  /// Unopened-file highlight base (fill uses ~110 alpha; edge opaque).
  /// Amber — unread/new cue, deliberately away from system selection blue.
  QString unopened_highlight = QStringLiteral("#E5A00D");
  /// Tint for hidden (dotfile) rows/tiles when shown (warm neutral gray).
  QString hidden_tint = QStringLiteral("#C9C4B8");
  /// Meta badge (fps / duration / resolution) background — supports #AARRGGBB.
  QString badge_background = QStringLiteral("#AAFFFFFF"); // white @ ~170/255
  /// Meta badge text.
  QString badge_foreground = QStringLiteral("#141414");
  /// File-cursor outline (Graphics view).
  QString cursor_outline = QStringLiteral("#000000");
  /// File-cursor fill (#AARRGGBB).
  QString cursor_fill = QStringLiteral("#60FFFFFF"); // white @ 96/255
  /// Directory montage white wash (#AARRGGBB).
  QString montage_wash = QStringLiteral("#A0FFFFFF"); // white @ 160/255
  /// Symlink emblem stroke/fill (teal — cool but not selection-blue).
  QString symlink_accent = QStringLiteral("#0F766E");
  /// Launch-flash fallback when palette Highlight is unavailable (violet).
  QString launch_flash = QStringLiteral("#8B5CF6");

  [[nodiscard]] static UiColors defaults() { return {}; }

  void load(QSettings& s);
  void save(QSettings& s) const;
  void reset_to_defaults() { *this = defaults(); }

  [[nodiscard]] QColor unopened_highlight_qcolor() const;
  [[nodiscard]] QColor hidden_tint_qcolor() const;
  [[nodiscard]] QColor badge_background_qcolor() const;
  [[nodiscard]] QColor badge_foreground_qcolor() const;
  [[nodiscard]] QColor cursor_outline_qcolor() const;
  [[nodiscard]] QColor cursor_fill_qcolor() const;
  [[nodiscard]] QColor montage_wash_qcolor() const;
  [[nodiscard]] QColor symlink_accent_qcolor() const;
  [[nodiscard]] QColor launch_flash_qcolor() const;
};

[[nodiscard]] QColor parse_ui_color(const QString& spec, const QColor& fallback);

} // namespace dirtoo::app

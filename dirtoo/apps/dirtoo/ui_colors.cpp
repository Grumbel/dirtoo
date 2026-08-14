// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_colors.hpp"

#include <QSettings>

namespace dirtoo::app {
namespace {

QString read_color(QSettings& s, const QString& key, const QString& def)
{
  const QString v = s.value(key, def).toString().trimmed();
  return v.isEmpty() ? def : v;
}

} // namespace

QColor parse_ui_color(const QString& spec, const QColor& fallback)
{
  if (spec.isEmpty()) {
    return fallback;
  }
  QColor c(spec);
  if (!c.isValid()) {
    // Accept RRGGBB without '#'.
    c = QColor(QStringLiteral("#") + spec);
  }
  return c.isValid() ? c : fallback;
}

void UiColors::load(QSettings& s)
{
  const UiColors d = defaults();
  s.beginGroup(QStringLiteral("ui/colors"));
  unopened_highlight = read_color(s, QStringLiteral("unopened_highlight"), d.unopened_highlight);
  hidden_tint = read_color(s, QStringLiteral("hidden_tint"), d.hidden_tint);
  badge_background = read_color(s, QStringLiteral("badge_background"), d.badge_background);
  badge_foreground = read_color(s, QStringLiteral("badge_foreground"), d.badge_foreground);
  cursor_outline = read_color(s, QStringLiteral("cursor_outline"), d.cursor_outline);
  cursor_fill = read_color(s, QStringLiteral("cursor_fill"), d.cursor_fill);
  montage_wash = read_color(s, QStringLiteral("montage_wash"), d.montage_wash);
  symlink_accent = read_color(s, QStringLiteral("symlink_accent"), d.symlink_accent);
  launch_flash = read_color(s, QStringLiteral("launch_flash"), d.launch_flash);
  s.endGroup();

  // Migrate single-key setting from early unopened-only preference.
  if (!s.contains(QStringLiteral("ui/colors/unopened_highlight"))
      && s.contains(QStringLiteral("ui/unopened_highlight_color"))) {
    const QString legacy = s.value(QStringLiteral("ui/unopened_highlight_color")).toString();
    if (!legacy.isEmpty()) {
      unopened_highlight = legacy;
    }
  }
}

void UiColors::save(QSettings& s) const
{
  s.beginGroup(QStringLiteral("ui/colors"));
  s.setValue(QStringLiteral("unopened_highlight"), unopened_highlight);
  s.setValue(QStringLiteral("hidden_tint"), hidden_tint);
  s.setValue(QStringLiteral("badge_background"), badge_background);
  s.setValue(QStringLiteral("badge_foreground"), badge_foreground);
  s.setValue(QStringLiteral("cursor_outline"), cursor_outline);
  s.setValue(QStringLiteral("cursor_fill"), cursor_fill);
  s.setValue(QStringLiteral("montage_wash"), montage_wash);
  s.setValue(QStringLiteral("symlink_accent"), symlink_accent);
  s.setValue(QStringLiteral("launch_flash"), launch_flash);
  s.endGroup();
}

QColor UiColors::unopened_highlight_qcolor() const
{
  return parse_ui_color(unopened_highlight, QColor(0x3b, 0x82, 0xf6));
}

QColor UiColors::hidden_tint_qcolor() const
{
  return parse_ui_color(hidden_tint, QColor(200, 200, 210));
}

QColor UiColors::badge_background_qcolor() const
{
  return parse_ui_color(badge_background, QColor(255, 255, 255, 170));
}

QColor UiColors::badge_foreground_qcolor() const
{
  return parse_ui_color(badge_foreground, QColor(20, 20, 20));
}

QColor UiColors::cursor_outline_qcolor() const
{
  return parse_ui_color(cursor_outline, QColor(0, 0, 0));
}

QColor UiColors::cursor_fill_qcolor() const
{
  return parse_ui_color(cursor_fill, QColor(255, 255, 255, 96));
}

QColor UiColors::montage_wash_qcolor() const
{
  return parse_ui_color(montage_wash, QColor(255, 255, 255, 160));
}

QColor UiColors::symlink_accent_qcolor() const
{
  return parse_ui_color(symlink_accent, QColor(30, 90, 200));
}

QColor UiColors::launch_flash_qcolor() const
{
  return parse_ui_color(launch_flash, QColor(80, 140, 255));
}

} // namespace dirtoo::app

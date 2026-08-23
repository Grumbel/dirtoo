// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QString>

namespace dirtoo::app {

[[nodiscard]] inline int group_header_height(const QFontMetrics& fm)
{
  return fm.height() + 10;
}

inline void paint_group_header(QPainter* painter, const QRect& rect, const QString& label,
                               const QPalette& palette, const QFont& base_font)
{
  if (painter == nullptr || rect.isEmpty() || label.isEmpty()) {
    return;
  }
  painter->save();

  QColor band = palette.color(QPalette::AlternateBase);
  if (band.lightness() > 128) {
    band = band.darker(106);
  } else {
    band = band.lighter(120);
  }
  painter->fillRect(rect, band);

  const QColor accent = palette.color(QPalette::Highlight);
  painter->fillRect(QRect(rect.left(), rect.top(), 3, rect.height()), accent);

  painter->setPen(QPen(palette.color(QPalette::Mid), 1));
  painter->drawLine(rect.bottomLeft(), rect.bottomRight());

  QFont font = base_font;
  font.setBold(true);
  painter->setFont(font);
  painter->setPen(palette.color(QPalette::WindowText));
  painter->drawText(rect.adjusted(10, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, label);

  painter->restore();
}

} // namespace dirtoo::app

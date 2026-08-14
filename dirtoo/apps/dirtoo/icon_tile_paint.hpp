// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cmath>

#include "badge_icons.hpp"
#include "file_list_model.hpp"

#include <QFileIconProvider>
#include <QFont>
#include <QIcon>
#include <QFontMetrics>
#include <QPainter>
#include <QRectF>
#include <QPen>
#include <QPolygonF>
#include <QPixmap>

namespace dirtoo::app {

/// Shared icon-tile painting used by GraphicsFileItem and FileItemDelegate so
/// directory montage / badges / status stickers cannot drift between views.

inline void paint_tile_badge(QPainter* painter, const QRect& thumb, const QString& text,
                             Qt::Alignment align)
{
  if (painter == nullptr || text.isEmpty() || thumb.isEmpty()) {
    return;
  }
  painter->save();
  // Scale badge with thumbnail so corners stay readable on large zoom levels.
  const qreal scale = std::clamp(thumb.width() / 96.0, 0.9, 2.75);
  QFont font = painter->font();
  const qreal base = font.pointSizeF() > 0 ? font.pointSizeF() : 10.0;
  font.setPointSizeF(std::max(8.0, base * 0.9 * scale));
  font.setBold(true);
  painter->setFont(font);
  const QFontMetrics fm(font);
  const int pad_x = std::max(3, static_cast<int>(std::lround(3 * scale)));
  const int pad_y = std::max(1, static_cast<int>(std::lround(1 * scale)));
  const int h = fm.height() + pad_y * 2;
  const int w = fm.horizontalAdvance(text) + pad_x * 2;
  QRect badge(0, 0, w, h);
  // Flush to thumbnail corners (1px inset only).
  if (align & Qt::AlignRight) {
    badge.moveRight(thumb.right());
  } else {
    badge.moveLeft(thumb.left());
  }
  if (align & Qt::AlignBottom) {
    badge.moveBottom(thumb.bottom());
  } else {
    badge.moveTop(thumb.top());
  }
  painter->setPen(Qt::NoPen);
  painter->setBrush(QColor(0, 0, 0, 160));
  painter->drawRoundedRect(badge, std::max(2.0, 2.0 * scale), std::max(2.0, 2.0 * scale));
  painter->setPen(QColor(255, 255, 255));
  painter->drawText(badge.adjusted(pad_x, 0, -pad_x, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
  painter->restore();
}

/// Unread-style cue for files not yet opened (when the preference is enabled).
inline void paint_unopened_indicator(QPainter* painter, const QRectF& br, const QColor& base)
{
  if (painter == nullptr || !br.isValid() || !base.isValid()) {
    return;
  }
  QColor fill = base;
  fill.setAlpha(110);
  painter->fillRect(br, fill);
  QColor edge = base;
  if (edge.alpha() < 200) {
    edge.setAlpha(255);
  }
  QPen pen(edge, 4);
  pen.setCapStyle(Qt::FlatCap);
  painter->setPen(pen);
  painter->drawLine(QPointF(br.left() + 2.0, br.top() + 2),
                    QPointF(br.left() + 2.0, br.bottom() - 2));
}

/// Whitened full-size folder glyph over a directory montage (hidden on hover).
inline void paint_directory_montage_overlay(QPainter* painter, const QRect& thumb)
{
  if (painter == nullptr || thumb.isEmpty()) {
    return;
  }
  painter->fillRect(thumb, QColor(255, 255, 255, 160));
  static QFileIconProvider provider;
  const QIcon folder_icon = provider.icon(QFileIconProvider::Folder);
  const int m = std::max(2, thumb.width() / 16);
  folder_icon.paint(painter, thumb.adjusted(m, m, -m, -m), Qt::AlignCenter);
}

inline void paint_status_pixmap(QPainter* painter, const QRect& thumb, const QPixmap& pm,
                                Qt::Alignment align, qreal opacity = 1.0, int size = 20)
{
  if (painter == nullptr || pm.isNull() || thumb.isEmpty()) {
    return;
  }
  QRect r(0, 0, size, size);
  if (align & Qt::AlignRight) {
    r.moveRight(thumb.right() - 2);
  } else {
    r.moveLeft(thumb.left() + 2);
  }
  if (align & Qt::AlignBottom) {
    r.moveBottom(thumb.bottom() - 2);
  } else {
    r.moveTop(thumb.top() + 2);
  }
  painter->save();
  painter->setOpacity(opacity);
  painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter->drawPixmap(r, pm.scaled(r.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  painter->restore();
}

/// Symlink emblem (bottom-left): theme emblem-symbolic-link or drawn arrow.
inline void paint_symlink_emblem(QPainter* painter, const QRect& thumb)
{
  if (painter == nullptr || thumb.isEmpty()) {
    return;
  }
  const int size = std::max(12, std::min(22, thumb.width() / 3));
  QRect r(0, 0, size, size);
  r.moveLeft(thumb.left() + 2);
  r.moveBottom(thumb.bottom() - 2);

  static const QIcon theme_icon = QIcon::fromTheme(QStringLiteral("emblem-symbolic-link"));
  if (!theme_icon.isNull()) {
    theme_icon.paint(painter, r, Qt::AlignCenter);
    return;
  }
  // Fallback: white disc + blue curved arrow hint.
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(Qt::NoPen);
  painter->setBrush(QColor(255, 255, 255, 210));
  painter->drawEllipse(r);
  painter->setPen(QPen(QColor(30, 90, 200), std::max(1.5, size / 10.0)));
  painter->setBrush(Qt::NoBrush);
  const QRectF arc = r.adjusted(size * 0.2, size * 0.15, -size * 0.15, -size * 0.2);
  painter->drawArc(arc, 40 * 16, 200 * 16);
  const QPointF tip(r.right() - size * 0.22, r.center().y() - size * 0.05);
  painter->setBrush(QColor(30, 90, 200));
  QPolygonF head;
  head << tip << QPointF(tip.x() - size * 0.28, tip.y() - size * 0.18)
       << QPointF(tip.x() - size * 0.28, tip.y() + size * 0.18);
  painter->drawPolygon(head);
  painter->restore();
}

/// Brief highlight after open/launch (LaunchFlashRole).
/// Graphics tiles also draw a rounded outline for stronger acknowledgement.
/// Accepts QRectF so GraphicsFileItem can pass boundingRect() without conversion.
inline void paint_launch_flash(QPainter* painter, const QRectF& rect, const QColor& highlight,
                               bool outline = false)
{
  if (painter == nullptr || rect.isEmpty()) {
    return;
  }
  QColor flash = highlight.isValid() ? highlight : QColor(80, 140, 255);
  flash.setAlpha(outline ? 160 : 150);
  painter->save();
  painter->fillRect(rect, flash);
  if (outline) {
    QPen pen(flash.darker(120), 2);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(rect.adjusted(1, 1, -1, -1), 4, 4);
  }
  painter->restore();
}

inline void paint_launch_flash(QPainter* painter, const QRect& rect, const QColor& highlight,
                               bool outline = false)
{
  paint_launch_flash(painter, QRectF(rect), highlight, outline);
}

/// New / loading / error / symlink stickers from model roles.
inline void paint_tile_status_overlays(QPainter* painter, const QRect& thumb,
                                       const QModelIndex& index)
{
  if (painter == nullptr || !index.isValid() || thumb.isEmpty()) {
    return;
  }
  if (index.data(IsNewRole).toBool()) {
    static const QPixmap k_new(load_badge_pixmap(QStringLiteral("badge-new.png")));
    paint_status_pixmap(painter, thumb, k_new, Qt::AlignLeft | Qt::AlignTop, 0.9);
  }
  // Permission stickers (bottom-right): unreadable takes precedence over unwritable.
  if (index.data(IsUnreadableRole).toBool()) {
    static const QPixmap k_ro(load_badge_pixmap(QStringLiteral("badge-readonly.png")));
    if (!k_ro.isNull()) {
      paint_status_pixmap(painter, thumb, k_ro, Qt::AlignRight | Qt::AlignBottom, 0.7);
    } else {
      // Legacy locked badge if new asset missing.
      static const QPixmap k_locked(load_badge_pixmap(QStringLiteral("badge-locked.png")));
      paint_status_pixmap(painter, thumb, k_locked, Qt::AlignRight | Qt::AlignBottom, 0.7);
    }
  } else if (index.data(IsUnwritableRole).toBool()) {
    static const QPixmap k_nw(load_badge_pixmap(QStringLiteral("badge-nowrite.png")));
    paint_status_pixmap(painter, thumb, k_nw, Qt::AlignRight | Qt::AlignBottom, 0.7);
  }
  if (index.data(IsSymlinkRole).toBool()) {
    paint_symlink_emblem(painter, thumb);
  }
  const auto status = static_cast<ThumbnailStatus>(index.data(ThumbnailStatusRole).toInt());
  if (status == ThumbnailStatus::Pending) {
    static const QPixmap k_loading(load_badge_pixmap(QStringLiteral("badge-loading.png")));
    paint_status_pixmap(painter, thumb, k_loading, Qt::AlignRight | Qt::AlignTop, 0.55);
  } else if (status == ThumbnailStatus::Failed) {
    static const QPixmap k_error(load_badge_pixmap(QStringLiteral("badge-error.png")));
    paint_status_pixmap(painter, thumb, k_error, Qt::AlignRight | Qt::AlignTop, 0.75);
  }
}

} // namespace dirtoo::app

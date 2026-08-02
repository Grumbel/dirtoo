// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "badge_icons.hpp"
#include "file_list_model.hpp"

#include <QFileIconProvider>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
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
  QFont font = painter->font();
  font.setPointSizeF(std::max(8.0, font.pointSizeF() * 0.85));
  painter->setFont(font);
  const QFontMetrics fm(font);
  const int pad_x = 3;
  const int h = fm.height() + 2;
  const int w = fm.horizontalAdvance(text) + pad_x * 2;
  QRect badge(0, 0, w, h);
  if (align & Qt::AlignRight) {
    badge.moveRight(thumb.right() - 1);
  } else {
    badge.moveLeft(thumb.left() + 1);
  }
  if (align & Qt::AlignBottom) {
    badge.moveBottom(thumb.bottom() - 1);
  } else {
    badge.moveTop(thumb.top() + 1);
  }
  painter->setPen(Qt::NoPen);
  painter->setBrush(QColor(255, 255, 255, 170));
  painter->drawRoundedRect(badge, 2, 2);
  painter->setPen(QColor(20, 20, 20));
  painter->drawText(badge.adjusted(pad_x, 0, -pad_x, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
  painter->restore();
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

/// New / loading / error stickers from model roles.
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

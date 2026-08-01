// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "graphics_file_item.hpp"

#include "file_list_model.hpp"
#include "graphics_file_view.hpp"

#include "dirtoo/filter/media_meta_cache.hpp"

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <algorithm>

namespace dirtoo::app {
namespace {

QString format_duration_ms(std::uint64_t ms)
{
  const int total = static_cast<int>(ms / 1000);
  const int h = total / 3600;
  const int m = (total % 3600) / 60;
  const int s = total % 60;
  if (h > 0) {
    return QStringLiteral("%1:%2:%3")
        .arg(h)
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'));
  }
  return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

void draw_badge(QPainter* painter, const QRect& thumb, const QString& text, Qt::Alignment align)
{
  if (text.isEmpty()) {
    return;
  }
  const QFontMetrics fm(painter->font());
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
  painter->setPen(Qt::black);
  painter->drawText(badge.adjusted(pad_x, 0, -pad_x, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
}

} // namespace

GraphicsFileItem::GraphicsFileItem(FileListModel* model, int row, GraphicsFileView* view)
    : model_(model)
    , view_(view)
    , row_(row)
{
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  setFlag(QGraphicsItem::ItemIsFocusable, true);
  setAcceptHoverEvents(true);
}

void GraphicsFileItem::set_row(int row)
{
  if (row_ == row) {
    return;
  }
  row_ = row;
  update();
}

QModelIndex GraphicsFileItem::model_index() const
{
  if (model_ == nullptr || row_ < 0) {
    return {};
  }
  return model_->index(row_, 0);
}

void GraphicsFileItem::set_tile_size(const QSize& size)
{
  if (tile_size_ == size) {
    return;
  }
  prepareGeometryChange();
  tile_size_ = size;
  update();
}

QRectF GraphicsFileItem::boundingRect() const
{
  return QRectF(0, 0, tile_size_.width(), tile_size_.height());
}

void GraphicsFileItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  (void)widget;
  if (model_ == nullptr || row_ < 0 || row_ >= model_->rowCount()) {
    return;
  }

  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

  const QRectF br = boundingRect();
  const bool selected = option != nullptr && (option->state & QStyle::State_Selected);
  const bool hover = option != nullptr && (option->state & QStyle::State_MouseOver);

  const QModelIndex idx = model_index();
  const QPalette pal = (widget != nullptr) ? widget->palette() : QPalette();
  const QColor highlight = pal.color(QPalette::Highlight);
  const QColor text_color = pal.color(QPalette::Text);
  const QColor base_color = pal.color(QPalette::Base);

  if (selected) {
    QColor fill = highlight;
    fill.setAlpha(90);
    painter->fillRect(br, fill);
  } else if (hover) {
    QColor fill = highlight;
    fill.setAlpha(40);
    painter->fillRect(br, fill);
  }

  // Group section header above the first item of a group (matches FileItemDelegate).
  int top_pad = 4;
  if (idx.data(IsGroupStartRole).toBool()) {
    const QString label = idx.data(GroupLabelRole).toString();
    if (!label.isEmpty()) {
      const QFontMetrics fm(painter->font());
      const int header_h = fm.height() + 8;
      QRect header_rect = br.toRect();
      header_rect.setHeight(header_h);
      painter->fillRect(header_rect, pal.color(QPalette::AlternateBase));
      painter->setPen(pal.color(QPalette::WindowText));
      painter->drawText(header_rect.adjusted(6, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, label);
      top_pad = header_h + 4;
    }
  }

  const QIcon icon = idx.data(Qt::DecorationRole).value<QIcon>();
  const QString text = idx.data(Qt::DisplayRole).toString();

  const int text_rows = model_->icon_text_rows();
  const int caption_h = text_rows > 0 ? (6 + text_rows * 16) : 0;
  int icon_side = std::min(tile_size_.width() - 8, tile_size_.height() - caption_h - top_pad - 4);
  icon_side = std::max(16, icon_side);
  QRect thumb(0, 0, icon_side, icon_side);
  thumb.moveCenter(QPoint(static_cast<int>(br.center().x()), top_pad + icon_side / 2));

  if (!icon.isNull()) {
    const QPixmap pm = icon.pixmap(QSize(icon_side * 2, icon_side * 2));
    if (!pm.isNull()) {
      if (model_->crop_thumbnails()) {
        const qreal sx = static_cast<qreal>(pm.width()) / thumb.width();
        const qreal sy = static_cast<qreal>(pm.height()) / thumb.height();
        const qreal scale = std::min(sx, sy);
        const int sw = static_cast<int>(thumb.width() * scale);
        const int sh = static_cast<int>(thumb.height() * scale);
        QRect src((pm.width() - sw) / 2, 0, sw, sh);
        painter->drawPixmap(thumb, pm, src);
      } else {
        const QSize scaled = pm.size().scaled(thumb.size(), Qt::KeepAspectRatio);
        QRect dest(0, 0, scaled.width(), scaled.height());
        dest.moveCenter(thumb.center());
        painter->drawPixmap(dest, pm);
      }
    } else {
      icon.paint(painter, thumb, Qt::AlignCenter);
    }
  }

  // Media badges from memory cache only
  if (const auto* fi = model_->file_at(row_); fi != nullptr && !fi->is_directory()) {
    if (const auto meta = filter::MediaMetaCache::instance().try_get(fi->path())) {
      QString top_left;
      QString top_right;
      QString bottom_left;
      if (meta->duration_ms && *meta->duration_ms > 0) {
        top_left = format_duration_ms(*meta->duration_ms);
      } else if (meta->pages && *meta->pages > 0) {
        top_left = QStringLiteral("%1 pages").arg(*meta->pages);
      } else if (meta->file_count && *meta->file_count > 0) {
        top_left = QStringLiteral("%1 files").arg(*meta->file_count);
      }
      if (meta->framerate && *meta->framerate > 0.0) {
        top_right = QStringLiteral("%1fps").arg(*meta->framerate, 0, 'g', 3);
      }
      if (meta->width && meta->height && *meta->width > 0 && *meta->height > 0) {
        bottom_left = QStringLiteral("%1×%2").arg(*meta->width).arg(*meta->height);
      }
      draw_badge(painter, thumb, top_left, Qt::AlignLeft | Qt::AlignTop);
      draw_badge(painter, thumb, top_right, Qt::AlignRight | Qt::AlignTop);
      draw_badge(painter, thumb, bottom_left, Qt::AlignLeft | Qt::AlignBottom);
    }
  }

  // Status roles
  if (idx.data(IsNewRole).toBool()) {
    static const QPixmap k_new(QStringLiteral(":/icons/badge-new.png"));
    if (!k_new.isNull()) {
      painter->setOpacity(0.9);
      painter->drawPixmap(QRect(thumb.left() + 2, thumb.top() + 2, 20, 20), k_new);
      painter->setOpacity(1.0);
    }
  }
  const auto status = static_cast<ThumbnailStatus>(idx.data(ThumbnailStatusRole).toInt());
  if (status == ThumbnailStatus::Pending) {
    static const QPixmap k_loading(QStringLiteral(":/icons/badge-loading.png"));
    if (!k_loading.isNull()) {
      painter->setOpacity(0.55);
      painter->drawPixmap(QRect(thumb.right() - 22, thumb.top() + 2, 20, 20), k_loading);
      painter->setOpacity(1.0);
    }
  } else if (status == ThumbnailStatus::Failed) {
    static const QPixmap k_error(QStringLiteral(":/icons/badge-error.png"));
    if (!k_error.isNull()) {
      painter->setOpacity(0.75);
      painter->drawPixmap(QRect(thumb.right() - 22, thumb.top() + 2, 20, 20), k_error);
      painter->setOpacity(1.0);
    }
  }

  // Caption — multi-line from model (icon detail level); theme-aware colors.
  if (!text.isEmpty() && caption_h > 0) {
    QRect text_rect = br.toRect().adjusted(4, thumb.bottom() + 2, -4, -2);
    if (selected) {
      QColor ht = pal.color(QPalette::HighlightedText);
      painter->setPen(ht.alpha() > 0 ? ht : text_color);
    } else {
      painter->setPen(text_color);
    }
    const QFontMetrics fm(painter->font());
    const QStringList lines = text.split(QLatin1Char('\n'));
    int y = text_rect.top();
    int drawn = 0;
    for (const QString& line : lines) {
      if (y + fm.height() > text_rect.bottom() || (text_rows > 0 && drawn >= text_rows)) {
        break;
      }
      const QString elided = fm.elidedText(line, Qt::ElideMiddle, text_rect.width());
      painter->drawText(QRect(text_rect.left(), y, text_rect.width(), fm.height()),
                        Qt::AlignHCenter | Qt::AlignTop, elided);
      y += fm.height() + 1;
      ++drawn;
    }
  }

  (void)base_color;
}

void GraphicsFileItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  if (view_ != nullptr) {
    view_->notify_activated(model_index());
  }
  QGraphicsItem::mouseDoubleClickEvent(event);
}

void GraphicsFileItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::MiddleButton && view_ != nullptr) {
    view_->notify_middle_clicked(model_index());
    event->accept();
    return;
  }
  QGraphicsItem::mousePressEvent(event);
}

void GraphicsFileItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  if (view_ != nullptr) {
    if (!isSelected()) {
      view_->select_row(row_, true);
    }
    view_->notify_context_menu(event->screenPos(), model_index());
    event->accept();
    return;
  }
  QGraphicsItem::contextMenuEvent(event);
}

} // namespace dirtoo::app

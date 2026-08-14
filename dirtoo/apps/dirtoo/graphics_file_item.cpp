// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "graphics_file_item.hpp"
#include "icon_tile_paint.hpp"
#include "badge_icons.hpp"
#include "tag_paint.hpp"

#include "file_list_model.hpp"
#include "graphics_file_view.hpp"

#include "dirtoo/filter/media_meta_cache.hpp"

#include <QFileIconProvider>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMetaObject>
#include <QDebug>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <algorithm>
#include <initializer_list>
#include <cctype>
#include <optional>

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

} // namespace

GraphicsFileItem::GraphicsFileItem(FileListModel* model, int row, GraphicsFileView* view)
    : model_(model)
    , view_(view)
    , row_(row)
{
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  setFlag(QGraphicsItem::ItemIsFocusable, true);
  setAcceptHoverEvents(true);
  if (model_ != nullptr && row_ >= 0) {
    setToolTip(model_->index(row_, 0).data(Qt::ToolTipRole).toString());
  }
}

void GraphicsFileItem::set_row(int row)
{
  if (row_ == row) {
    return;
  }
  row_ = row;
  if (model_ != nullptr && row_ >= 0) {
    setToolTip(model_->index(row_, 0).data(Qt::ToolTipRole).toString());
  } else {
    setToolTip({});
  }
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

void GraphicsFileItem::set_drop_target(bool on)
{
  if (drop_target_ == on) {
    return;
  }
  drop_target_ = on;
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
  } else if (idx.data(IsHiddenRole).toBool()) {
    // Distinct tile background for hidden (dot) files when shown.
    painter->fillRect(br, QColor(200, 200, 210));
  }
  // Unread-mail style: files not yet opened (under/over selection so the edge stays visible).
  if (model_->show_opened_state() && !idx.data(IsOpenedRole).toBool()) {
    paint_unopened_indicator(painter, br, model_->unopened_highlight_color());
  }
  // Brief flash after open/launch so slow app start still feels acknowledged.
  if (idx.data(LaunchFlashRole).toBool()) {
    paint_launch_flash(painter, br, highlight, /*outline=*/true);
  }
  if (drop_target_) {
    QColor fill = highlight;
    fill.setAlpha(120);
    painter->fillRect(br, fill);
    QPen pen(highlight, 2);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(br.adjusted(1, 1, -1, -1), 4, 4);
  }

  // File cursor outline (dirtoo-py is_cursor): light fill + black border, independent of selection.
  if (view_ != nullptr && view_->is_cursor_row(row_)) {
    painter->setOpacity(1.0);
    painter->setPen(QPen(QColor(0, 0, 0), 1));
    painter->setBrush(QColor(255, 255, 255, 96));
    painter->drawRect(br.adjusted(0.5, 0.5, -0.5, -0.5));
  }

  // Group headers: full-width band in GraphicsFileView::drawForeground.
  const int top_pad = 1;
  const int side_margin = 1;

  const QIcon icon = idx.data(Qt::DecorationRole).value<QIcon>();
  QString text = idx.data(Qt::DisplayRole).toString();
  if (text.isEmpty()) {
    if (const auto* fi = model_->file_at(row_)) {
      text = QString::fromStdString(fi->basename());
    }
  }

  const int text_rows = model_->icon_text_rows();
  // +1 line budget so a long basename can wrap without eating meta lines.
  const int caption_h =
      text_rows > 0 ? (4 + (text_rows + 1) * 16)
                    : (model_->icon_detail_level() > 0 ? 20 : 0);
  // Fill the tile width; vertical room left for caption under the image.
  const int avail_h = std::max(16, tile_size_.height() - caption_h - top_pad - 2);
  int icon_side = std::min(tile_size_.width() - 2 * side_margin, avail_h);
  icon_side = std::max(16, icon_side);
  // Prefer full width when the tile is wider than tall (caption-only extra height).
  if (tile_size_.width() - 2 * side_margin <= avail_h) {
    icon_side = tile_size_.width() - 2 * side_margin;
  }
  QRect thumb(side_margin, top_pad, icon_side, icon_side);

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

  const fs::FileInfo* fi = model_->file_at(row_);

  // Directory montage overlay (shared with FileItemDelegate).
  if (fi != nullptr && fi->is_directory()
      && idx.data(ThumbnailStatusRole).toInt() == static_cast<int>(ThumbnailStatus::Ready)
      && !hover) {
    paint_directory_montage_overlay(painter, thumb);
  }

  // Non-recursive file count for folders.
  if (fi != nullptr && fi->is_directory()) {
    const qint64 n = idx.data(ChildCountRole).toLongLong();
    if (n >= 0) {
      paint_tile_badge(painter, thumb, QStringLiteral("%1").arg(n), Qt::AlignRight | Qt::AlignBottom);
    }
  }

  // Image/video type stickers always; text meta when detail > 1 (dirtoo-py).
  if (fi != nullptr && !fi->is_directory()) {
    std::string ext = fi->extension();
    if (!ext.empty() && ext[0] == '.') {
      ext.erase(ext.begin());
    }
    for (char& c : ext) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    auto is_one_of = [&](std::initializer_list<const char*> list) {
      for (const char* e : list) {
        if (ext == e) {
          return true;
        }
      }
      return false;
    };
    const bool is_image = is_one_of(
        {"png", "jpg", "jpeg", "gif", "bmp", "webp", "tif", "tiff", "svg", "heic", "avif"});
    const bool is_video = is_one_of(
        {"mp4", "mkv", "webm", "avi", "mov", "wmv", "m4v", "mpeg", "mpg", "ts", "flv"});
    const bool is_audio = is_one_of(
        {"mp3", "flac", "ogg", "opus", "wav", "m4a", "aac", "wma"});
    const bool want_meta = is_image || is_video || is_audio
        || is_one_of({"pdf", "zip", "tar", "tgz", "7z", "rar", "cbz", "cbr", "jar", "apk", "gz",
                      "bz2", "xz"});

    if (model_->icon_detail_level() > 1 && want_meta) {
      auto& cache = filter::MediaMetaCache::instance();
      auto meta = cache.try_get(fi->path());
      if (!meta && !cache.is_negative(fi->path())) {
        const int row = row_;
        FileListModel* model = model_;
        cache.request(fi->path(), cache.generation(),
                      [model, row](const std::string&, std::optional<filter::MediaInfo>,
                                   std::uint64_t) {
                        if (model == nullptr) {
                          return;
                        }
                        QMetaObject::invokeMethod(model, "notify_row_changed", Qt::QueuedConnection,
                                                  Q_ARG(int, row));
                      });
      }
      QString top_left;
      QString top_right;
      QString bottom_left;
      if (meta) {
        if ((is_video || is_audio) && meta->duration_ms && *meta->duration_ms >= 1000) {
          top_left = format_duration_ms(*meta->duration_ms);
        } else if (meta->pages && *meta->pages > 0) {
          top_left = QStringLiteral("%1 pages").arg(*meta->pages);
        } else if (meta->file_count && *meta->file_count > 0) {
          top_left = QStringLiteral("%1 files").arg(*meta->file_count);
        }
        if ((is_video || is_audio) && meta->framerate && *meta->framerate > 0.0) {
          top_right = QStringLiteral("%1fps").arg(*meta->framerate, 0, 'g', 3);
        }
        if (meta->width && meta->height && *meta->width > 0 && *meta->height > 0) {
          bottom_left = QStringLiteral("%1×%2").arg(*meta->width).arg(*meta->height);
        }
      }
      paint_tile_badge(painter, thumb, top_left, Qt::AlignLeft | Qt::AlignTop);
      paint_tile_badge(painter, thumb, top_right, Qt::AlignRight | Qt::AlignTop);
      paint_tile_badge(painter, thumb, bottom_left, Qt::AlignLeft | Qt::AlignBottom);
    }

    if (fi != nullptr) {
      paint_tag_chips(painter, thumb, fi->path());
    }

    // Type sticker: bottom-right (Python paint_metadata / SharedPixmaps).
    if (is_image || is_video || is_audio) {
      static QPixmap k_video(load_badge_pixmap(QStringLiteral("badge-video.png")));
      static QPixmap k_image(load_badge_pixmap(QStringLiteral("badge-image.png")));
      static bool logged = false;
      if (!logged) {
        logged = true;
        if (k_video.isNull() || k_image.isNull()) {
          qWarning("dirtoo: type stickers failed to load from :/icons/badge-{video,image}.png "
                   "(null video=%d image=%d)",
                   int(k_video.isNull()), int(k_image.isNull()));
        } else {
          qInfo("dirtoo: type stickers loaded (video %dx%d, image %dx%d)",
                k_video.width(), k_video.height(), k_image.width(), k_image.height());
        }
      }
      const QPixmap& pm = (is_video || is_audio) ? k_video : k_image;
      if (!pm.isNull() && !thumb.isEmpty()) {
        // Match Python: 24x24 in thumbnail corner, opacity ~0.5 — use 0.75 for visibility.
        const int s = std::min(28, std::max(18, thumb.width() / 4));
        const QRect r(thumb.right() - s - 2, thumb.bottom() - s - 2, s, s);
        painter->save();
        painter->setOpacity(0.75);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawPixmap(r, pm.scaled(r.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        painter->restore();
      }
    }
  }

  paint_tile_status_overlays(painter, thumb, idx);

  // Captions: basename in normal text color; size/date in gray (dirtoo-py).
  // Long names wrap to a second line when the tile budget allows, else elide.
  if (!text.isEmpty() && caption_h > 0) {
    QRect text_rect = br.toRect().adjusted(4, thumb.bottom() + 2, -4, -2);
    const QColor name_color = text_color.isValid() ? text_color : QColor(0, 0, 0);
    const QColor secondary(96, 96, 96);
    const QFontMetrics fm(painter->font());
    QStringList parts = text.split(QLatin1Char('\n'));
    QStringList paint_lines;
    if (!parts.isEmpty()) {
      const QString name = parts.front();
      const bool can_wrap = text_rows >= 1 && text_rect.width() > 24
                            && fm.horizontalAdvance(name) > text_rect.width();
      if (can_wrap) {
        int brk = name.size() / 2;
        for (int i = brk; i < name.size() && i < brk + 12; ++i) {
          const QChar ch = name[i];
          if (ch == QLatin1Char('-') || ch == QLatin1Char('_') || ch == QLatin1Char('.')
              || ch == QLatin1Char(' ')) {
            brk = i + 1;
            break;
          }
        }
        brk = std::clamp(brk, 1, static_cast<int>(name.size()) - 1);
        paint_lines << name.left(brk);
        paint_lines << name.mid(brk);
      } else {
        paint_lines << name;
      }
      for (int i = 1; i < parts.size(); ++i) {
        paint_lines << parts[i];
      }
    }
    const int name_line_count = (paint_lines.size() >= 2 && parts.size() == 1) ? 2 : 1;
    int y = text_rect.top();
    int drawn = 0;
    const int budget = std::max(text_rows, name_line_count + std::max(0, text_rows - 1));
    for (const QString& line : paint_lines) {
      if (y + fm.height() > text_rect.bottom() || drawn >= budget) {
        break;
      }
      const QString elided = fm.elidedText(line, Qt::ElideMiddle, text_rect.width());
      const bool is_name = drawn < name_line_count;
      painter->setPen(is_name ? name_color : secondary);
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
  // Left-button: tag chip click filters by that tag; otherwise selection is owned
  // by GraphicsFileView (deferred multi-select drag, Ctrl/Shift range).
  if (event->button() == Qt::LeftButton) {
    if (view_ != nullptr && model_ != nullptr && row_ >= 0) {
      const fs::FileInfo* fi = model_->file_at(row_);
      if (fi != nullptr && !fi->is_directory()) {
        const QRectF br = boundingRect();
        const int top_pad = 4;
        const int text_rows = model_->icon_text_rows();
        const int caption_h =
            text_rows > 0 ? (6 + text_rows * 16)
                          : (model_->icon_detail_level() > 0 ? 22 : 0);
        const int side_margin = 1;
        const int top_pad = 1;
        const int avail_h = std::max(16, tile_size_.height() - caption_h - top_pad - 2);
        int icon_side = std::min(tile_size_.width() - 2 * side_margin, avail_h);
        icon_side = std::max(16, icon_side);
        if (tile_size_.width() - 2 * side_margin <= avail_h) {
          icon_side = tile_size_.width() - 2 * side_margin;
        }
        QRect thumb(side_margin, top_pad, icon_side, icon_side);
        const QPoint local = event->pos().toPoint();
        const QString tag = tag_chip_at(thumb, fi->path(), local);
        if (!tag.isEmpty()) {
          view_->notify_tag_chip_clicked(tag);
          event->accept();
          return;
        }
      }
    }
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

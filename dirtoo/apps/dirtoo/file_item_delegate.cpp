// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "file_item_delegate.hpp"

#include "file_list_model.hpp"
#include "dirtoo/filter/media_meta_cache.hpp"
#include "dirtoo/fs/file_info.hpp"

#include <QApplication>
#include <QFontMetrics>
#include <QMetaObject>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <cmath>
#include <cctype>
#include <optional>

namespace dirtoo::app {
namespace {

void draw_badge(QPainter* painter, const QRect& thumb, const QString& text, Qt::Alignment align)
{
  if (text.isEmpty()) {
    return;
  }
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
}

QString format_duration_ms(std::uint64_t ms)
{
  const auto total_s = static_cast<int>(ms / 1000);
  const int h = total_s / 3600;
  const int m = (total_s % 3600) / 60;
  const int s = total_s % 60;
  if (h > 0) {
    return QStringLiteral("%1:%2:%3")
        .arg(h)
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'));
  }
  return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

enum class MediaKind { None, Image, Video, Audio };

MediaKind classify_extension(const std::string& ext_in)
{
  std::string ext = ext_in;
  if (!ext.empty() && ext[0] == '.') {
    ext.erase(ext.begin());
  }
  for (char& c : ext) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  static const char* images[] = {"png", "jpg", "jpeg", "gif", "bmp", "webp", "tif", "tiff", "svg",
                                 "heic", "avif", nullptr};
  static const char* videos[] = {"mp4", "mkv", "webm", "avi", "mov", "wmv", "m4v", "mpeg", "mpg",
                                 "ts", "flv", nullptr};
  static const char* audio[] = {"mp3", "flac", "ogg", "opus", "wav", "m4a", "aac", "wma", nullptr};
  auto match = [&](const char** list) {
    for (int i = 0; list[i] != nullptr; ++i) {
      if (ext == list[i]) {
        return true;
      }
    }
    return false;
  };
  if (match(images)) {
    return MediaKind::Image;
  }
  if (match(videos)) {
    return MediaKind::Video;
  }
  if (match(audio)) {
    return MediaKind::Audio;
  }
  return MediaKind::None;
}

void draw_type_badge(QPainter* painter, const QRect& thumb, MediaKind kind)
{
  if (kind == MediaKind::None) {
    return;
  }
  const int s = 18;
  QRect r(thumb.right() - s - 3, thumb.bottom() - s - 3, s, s);
  painter->save();
  painter->setOpacity(0.85);
  painter->setPen(Qt::NoPen);
  if (kind == MediaKind::Video) {
    painter->setBrush(QColor(40, 40, 40, 200));
    painter->drawRoundedRect(r, 3, 3);
    // play triangle
    painter->setBrush(Qt::white);
    QPainterPath tri;
    tri.moveTo(r.left() + 6, r.top() + 4);
    tri.lineTo(r.right() - 4, r.center().y());
    tri.lineTo(r.left() + 6, r.bottom() - 4);
    tri.closeSubpath();
    painter->drawPath(tri);
  } else if (kind == MediaKind::Image) {
    painter->setBrush(QColor(40, 40, 40, 200));
    painter->drawRoundedRect(r, 3, 3);
    painter->setPen(QPen(Qt::white, 1.2));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(r.adjusted(4, 5, -4, -4));
    painter->setBrush(Qt::white);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QPoint(r.left() + 7, r.top() + 8), 2, 2);
  } else if (kind == MediaKind::Audio) {
    painter->setBrush(QColor(40, 40, 40, 200));
    painter->drawRoundedRect(r, 3, 3);
    painter->setPen(QPen(Qt::white, 1.5));
    const int cx = r.center().x();
    painter->drawLine(cx - 2, r.top() + 5, cx - 2, r.bottom() - 5);
    painter->drawLine(cx + 2, r.top() + 7, cx + 2, r.bottom() - 5);
    painter->drawLine(cx + 5, r.top() + 9, cx + 5, r.bottom() - 5);
  }
  painter->restore();
}

} // namespace

FileItemDelegate::FileItemDelegate(FileListModel* model, QObject* parent)
    : QStyledItemDelegate(parent)
    , model_(model)
{
}

QSize FileItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  return QStyledItemDelegate::sizeHint(option, index);
}

void FileItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
  if (model_ == nullptr || !model_->icon_style_active()) {
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);
  painter->setClipRect(opt.rect);

  // Selection / hover background
  if (opt.state & QStyle::State_Selected) {
    painter->fillRect(opt.rect, opt.palette.highlight());
  } else if (opt.state & QStyle::State_MouseOver) {
    QColor c = opt.palette.highlight().color();
    c.setAlpha(40);
    painter->fillRect(opt.rect, c);
  }

  const QIcon icon = opt.icon;
  const QString text = opt.text;
  const int text_rows = model_ != nullptr ? model_->icon_text_rows() : 0;
  const int caption_budget = text_rows > 0 ? (6 + text_rows * 18) : 0;
  int icon_side = opt.decorationSize.width() > 0 ? opt.decorationSize.width()
                                                 : std::min(opt.rect.width() - 8, opt.rect.height() / 2);
  // Keep enough vertical space under the icon for the caption lines.
  icon_side = std::min(icon_side, std::max(16, opt.rect.height() - caption_budget - 8));
  icon_side = std::min(icon_side, opt.rect.width() - 8);
  QRect thumb(0, 0, icon_side, icon_side);
  thumb.moveCenter(QPoint(opt.rect.center().x(), opt.rect.top() + icon_side / 2 + 4));

  // Icon / thumbnail
  if (!icon.isNull()) {
    icon.paint(painter, thumb, Qt::AlignCenter, QIcon::Normal, QIcon::On);
  }

  const fs::FileInfo* fi = model_->file_at(index.row());
  MediaKind kind = MediaKind::None;
  std::optional<filter::MediaInfo> meta;

  if (fi != nullptr && !fi->is_directory()) {
    kind = classify_extension(fi->extension());
    // Memory-only lookup on the GUI thread. Missing media is requested async.
    if (kind != MediaKind::None) {
      auto& cache = filter::MediaMetaCache::instance();
      meta = cache.try_get(fi->path());
      if (!meta && !cache.is_negative(fi->path())) {
        const QString path_key = QString::fromStdString(fi->path().string());
        const int row = index.row();
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
    }

    if (model_->icon_detail_level() > 1) {
      QString top_left;
      QString top_right;
      QString bottom_left;

      if (meta && meta->duration_ms && *meta->duration_ms > 0) {
        top_left = format_duration_ms(*meta->duration_ms);
      }
      if (meta && meta->framerate && *meta->framerate > 0.0) {
        top_right = QStringLiteral("%1fps").arg(*meta->framerate, 0, 'g', 3);
      }
      if (meta && meta->width && meta->height && *meta->width > 0 && *meta->height > 0) {
        bottom_left = QStringLiteral("%1×%2").arg(*meta->width).arg(*meta->height);
      }

      draw_badge(painter, thumb, top_left, Qt::AlignLeft | Qt::AlignTop);
      draw_badge(painter, thumb, top_right, Qt::AlignRight | Qt::AlignTop);
      draw_badge(painter, thumb, bottom_left, Qt::AlignLeft | Qt::AlignBottom);
      draw_type_badge(painter, thumb, kind);
    } else if (kind != MediaKind::None) {
      draw_type_badge(painter, thumb, kind);
    }
  }

  // Caption under thumbnail — elide each line so long names/sizes stay inside the cell.
  if (!text.isEmpty() && model_->icon_detail_level() > 0) {
    QRect text_rect = opt.rect.adjusted(4, thumb.bottom() + 4, -4, -2);
    if (text_rect.height() > 0 && text_rect.width() > 0) {
      QPen pen = painter->pen();
      if (opt.state & QStyle::State_Selected) {
        painter->setPen(opt.palette.highlightedText().color());
      } else {
        painter->setPen(opt.palette.text().color());
      }
      const QFontMetrics fm(painter->font());
      const QStringList lines = text.split(QLatin1Char('\n'));
      int y = text_rect.top();
      for (const QString& line : lines) {
        if (y + fm.height() > text_rect.bottom()) {
          break;
        }
        const QString elided = fm.elidedText(line, Qt::ElideMiddle, text_rect.width());
        painter->drawText(QRect(text_rect.left(), y, text_rect.width(), fm.height()),
                          Qt::AlignHCenter | Qt::AlignVCenter, elided);
        y += fm.height() + 1;
      }
      painter->setPen(pen);
    }
  }

  painter->restore();
}

} // namespace dirtoo::app

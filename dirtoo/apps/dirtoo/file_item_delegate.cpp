// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "file_item_delegate.hpp"
#include <QMouseEvent>
#include <QEvent>
#include "icon_tile_paint.hpp"
#include "group_header_paint.hpp"
#include "badge_icons.hpp"
#include "tag_paint.hpp"

#include "file_list_model.hpp"
#include "dirtoo/filter/media_meta_cache.hpp"
#include "dirtoo/fs/file_info.hpp"

#include <QApplication>
#include <QColor>
#include <QFileIconProvider>
#include <QFontMetrics>
#include <QMetaObject>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <optional>

namespace dirtoo::app {
namespace {

/// Threshold (seconds) above which a time-gap separator is drawn between rows.
constexpr qint64 kTimeGapThresholdSecs = 6 * 60 * 60; // 6 hours

QString format_time_gap(qint64 secs)
{
  if (secs < 60) {
    return QStringLiteral("%1s gap").arg(secs);
  }
  if (secs < 3600) {
    return QStringLiteral("%1m gap").arg(secs / 60);
  }
  if (secs < 86400) {
    return QStringLiteral("%1h gap").arg(secs / 3600);
  }
  return QStringLiteral("%1d gap").arg(secs / 86400);
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

const QPixmap& badge_pixmap(MediaKind kind)
{
  static const QPixmap k_video(load_badge_pixmap(QStringLiteral("badge-video.png")));
  static const QPixmap k_image(load_badge_pixmap(QStringLiteral("badge-image.png")));
  static const QPixmap k_empty;
  if (kind == MediaKind::Video || kind == MediaKind::Audio) {
    return k_video;
  }
  if (kind == MediaKind::Image) {
    return k_image;
  }
  return k_empty;
}


void draw_type_badge(QPainter* painter, const QRect& thumb, MediaKind kind)
{
  if (kind == MediaKind::None || thumb.isEmpty()) {
    return;
  }
  const QPixmap& pm = badge_pixmap(kind);
  if (pm.isNull()) {
    return;
  }
  const int s = std::min(28, std::max(18, thumb.width() / 4));
  const QRect r(thumb.right() - s - 2, thumb.bottom() - s - 2, s, s);
  painter->save();
  painter->setOpacity(0.75);
  painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter->drawPixmap(r, pm.scaled(r.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
  // List view (icon left of name): row a bit taller than icon/font; width ≈ column.
  if (model_ != nullptr && !model_->icon_style_active()) {
    const int icon = option.decorationSize.height() > 0 ? option.decorationSize.height() : 16;
    const int text_h = option.fontMetrics.height();
    int h = std::max(icon, text_h) + 6;
    if (index.isValid() && index.data(IsGroupStartRole).toBool()
        && !index.data(GroupLabelRole).toString().isEmpty()) {
      h += group_header_height(option.fontMetrics);
    }
    if (index.isValid()) {
      const qint64 gap = index.data(TimeGapSecondsRole).toLongLong();
      if (gap >= kTimeGapThresholdSecs) {
        h += option.fontMetrics.height() + 6;
      }
    }
    const int w = std::max(option.decorationSize.width() + 12 + 140, 180);
    return QSize(w, h);
  }

  QSize sz = QStyledItemDelegate::sizeHint(option, index);
  if (index.isValid() && index.data(IsGroupStartRole).toBool()) {
    const QString label = index.data(GroupLabelRole).toString();
    if (!label.isEmpty()) {
      sz.setHeight(sz.height() + group_header_height(option.fontMetrics));
    }
  }
  if (index.isValid()) {
    const qint64 gap = index.data(TimeGapSecondsRole).toLongLong();
    if (gap >= kTimeGapThresholdSecs) {
      sz.setHeight(sz.height() + option.fontMetrics.height() + 6);
    }
  }
  return sz;
}

void FileItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);

  // Group section header (day / directory) above the first item of a group.
  if (index.isValid() && index.data(IsGroupStartRole).toBool()) {
    const QString label = index.data(GroupLabelRole).toString();
    if (!label.isEmpty()) {
      const int header_h = group_header_height(option.fontMetrics);
      QRect header_rect = opt.rect;
      header_rect.setHeight(header_h);
      paint_group_header(painter, header_rect, label, option.palette, option.font);
      opt.rect.setTop(opt.rect.top() + header_h);
    }
  }

  // Time-gap separator when consecutive items are far apart in mtime.
  if (index.isValid()) {
    const qint64 gap = index.data(TimeGapSecondsRole).toLongLong();
    if (gap >= kTimeGapThresholdSecs) {
      const int gap_h = option.fontMetrics.height() + 6;
      QRect gap_rect = opt.rect;
      gap_rect.setHeight(gap_h);
      painter->save();
      painter->fillRect(gap_rect, option.palette.mid().color().lighter(130));
      painter->setPen(option.palette.color(QPalette::PlaceholderText));
      QFont f = option.font;
      f.setItalic(true);
      f.setPointSizeF(std::max(8.0, f.pointSizeF() - 1.0));
      painter->setFont(f);
      painter->drawText(gap_rect.adjusted(8, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft,
                        format_time_gap(gap));
      painter->restore();
      opt.rect.setTop(opt.rect.top() + gap_h);
    }
  }

  // Detail / List: square icon (optionally cropped) left of the name.
  if (model_ == nullptr || !model_->icon_style_active()) {
    if (index.column() != 0) {
      QStyledItemDelegate::paint(painter, opt, index);
      return;
    }
    const QStyle* style = opt.widget != nullptr ? opt.widget->style() : QApplication::style();
    // Hidden-file row tint before the style panel (selection still wins via state).
    if (!(opt.state & QStyle::State_Selected) && index.data(IsHiddenRole).toBool()) {
      painter->fillRect(opt.rect, QColor(200, 200, 210));
    }
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
    if (model_ != nullptr && model_->show_opened_state() && !index.data(IsOpenedRole).toBool()) {
      paint_unopened_indicator(painter, opt.rect, model_->unopened_highlight_color());
    }
    if (index.data(LaunchFlashRole).toBool()) {
      paint_launch_flash(painter, opt.rect, opt.palette.color(QPalette::Highlight));
    }

    const int icon = opt.decorationSize.isValid()
                         ? std::max(opt.decorationSize.width(), opt.decorationSize.height())
                         : 16;
    const int side = std::min(icon, std::max(12, opt.rect.height() - 4));
    QRect thumb(opt.rect.left() + 2, opt.rect.top() + (opt.rect.height() - side) / 2, side, side);

    QIcon icon_obj = opt.icon;
    if (icon_obj.isNull() && index.isValid()) {
      icon_obj = index.data(Qt::DecorationRole).value<QIcon>();
    }
    if (!icon_obj.isNull()) {
      const QPixmap pm = icon_obj.pixmap(QSize(side * 2, side * 2));
      if (!pm.isNull()) {
        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->setClipRect(thumb);
        if (model_ != nullptr && model_->crop_thumbnails()) {
          // Cover: fill square, crop overflow.
          const qreal sx = static_cast<qreal>(pm.width()) / static_cast<qreal>(thumb.width());
          const qreal sy = static_cast<qreal>(pm.height()) / static_cast<qreal>(thumb.height());
          const qreal scale = std::min(sx, sy);
          QRectF srcrect(0, 0, thumb.width() * scale, thumb.height() * scale);
          srcrect.moveCenter(QRectF(pm.rect()).center());
          if (srcrect.left() < 0) {
            srcrect.moveLeft(0);
          }
          if (srcrect.top() < 0) {
            srcrect.moveTop(0);
          }
          if (srcrect.right() > pm.width()) {
            srcrect.setWidth(pm.width());
          }
          if (srcrect.bottom() > pm.height()) {
            srcrect.setHeight(pm.height());
          }
          painter->drawPixmap(thumb, pm, srcrect.toRect());
        } else {
          // Fit inside square (letterbox).
          const QSize scaled = pm.size().scaled(thumb.size(), Qt::KeepAspectRatio);
          QRect dest(0, 0, scaled.width(), scaled.height());
          dest.moveCenter(thumb.center());
          painter->fillRect(thumb, opt.palette.base());
          painter->drawPixmap(dest, pm);
        }
        painter->restore();
      }
    }

    QString text = opt.text;
    if (text.isEmpty() && index.isValid()) {
      text = index.data(Qt::DisplayRole).toString();
    }
    if (!text.isEmpty()) {
      QRect text_rect = opt.rect.adjusted(thumb.right() + 6 - opt.rect.left(), 0, -4, 0);
      text_rect.setLeft(thumb.right() + 6);
      const QFontMetrics fm(opt.font);
      const QString elided = fm.elidedText(text, Qt::ElideMiddle, text_rect.width());
      painter->setPen(opt.palette.color(opt.state & QStyle::State_Selected ? QPalette::HighlightedText
                                                                         : QPalette::Text));
      painter->drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, elided);
    }
    return;
  }

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

  painter->setClipRect(opt.rect);

  // Selection / hover background; muted tile for hidden (dot) files.
  if (opt.state & QStyle::State_Selected) {
    painter->fillRect(opt.rect, opt.palette.highlight());
  } else if (opt.state & QStyle::State_MouseOver) {
    QColor c = opt.palette.highlight().color();
    c.setAlpha(40);
    painter->fillRect(opt.rect, c);
  } else if (index.data(IsHiddenRole).toBool()) {
    painter->fillRect(opt.rect, QColor(200, 200, 210));
  }
  if (model_ != nullptr && model_->show_opened_state() && !index.data(IsOpenedRole).toBool()) {
    paint_unopened_indicator(painter, opt.rect, model_->unopened_highlight_color());
  }
  if (index.data(LaunchFlashRole).toBool()) {
    paint_launch_flash(painter, opt.rect, opt.palette.color(QPalette::Highlight));
  }

  const QIcon icon = opt.icon;
  QString text = opt.text;
  if (text.isEmpty() && model_ != nullptr) {
    if (const auto* fi = model_->file_at(index.row())) {
      text = QString::fromStdString(fi->basename());
    }
  }
  const int text_rows = model_ != nullptr ? model_->icon_text_rows() : 0;
  const int caption_budget =
      text_rows > 0 ? (6 + text_rows * 18)
                    : (model_ != nullptr && model_->icon_detail_level() > 0 ? 22 : 0);
  // Dense: fill cell width; leave caption_budget under the image.
  const int side_margin = 1;
  const int top_pad = 1;
  const int avail_h = std::max(16, opt.rect.height() - caption_budget - top_pad - 2);
  int icon_side = std::min(opt.rect.width() - 2 * side_margin, avail_h);
  icon_side = std::max(16, icon_side);
  if (opt.rect.width() - 2 * side_margin <= avail_h) {
    icon_side = opt.rect.width() - 2 * side_margin;
  }
  QRect thumb(opt.rect.left() + side_margin, opt.rect.top() + top_pad, icon_side, icon_side);

  // Icon / thumbnail — letterbox (fit) vs cover (crop), matching Python crop_thumbnails.
  if (!icon.isNull()) {
    const QPixmap pm = icon.pixmap(QSize(std::max(thumb.width(), thumb.height()) * 2,
                                         std::max(thumb.width(), thumb.height()) * 2));
    if (pm.isNull()) {
      icon.paint(painter, thumb, Qt::AlignCenter, QIcon::Normal, QIcon::On);
    } else if (model_ != nullptr && model_->crop_thumbnails()) {
      // Cover: scale to fill thumb, crop overflow (Python make_cropped_rect + drawPixmap).
      const qreal sx = static_cast<qreal>(pm.width()) / static_cast<qreal>(thumb.width());
      const qreal sy = static_cast<qreal>(pm.height()) / static_cast<qreal>(thumb.height());
      const qreal scale = std::min(sx, sy); // crop the larger dimension
      const int sw = static_cast<int>(thumb.width() * scale);
      const int sh = static_cast<int>(thumb.height() * scale);
      QRect srcrect((pm.width() - sw) / 2, 0, sw, sh); // top-aligned crop like Python
      if (srcrect.height() > pm.height()) {
        srcrect.setHeight(pm.height());
      }
      if (srcrect.width() > pm.width()) {
        srcrect.setWidth(pm.width());
      }
      painter->drawPixmap(thumb, pm, srcrect);
    } else {
      // Fit: whole pixmap visible inside thumb (letterbox).
      const QSize scaled = pm.size().scaled(thumb.size(), Qt::KeepAspectRatio);
      QRect dest(0, 0, scaled.width(), scaled.height());
      dest.moveCenter(thumb.center());
      painter->drawPixmap(dest, pm);
    }
  }

  const fs::FileInfo* fi = model_->file_at(index.row());
  const bool is_dir = fi != nullptr && fi->is_directory();
  const bool hover = (opt.state & QStyle::State_MouseOver);

  // Directory montage overlay (shared with GraphicsFileItem).
  if (is_dir
      && index.data(ThumbnailStatusRole).toInt() == static_cast<int>(ThumbnailStatus::Ready)
      && !hover) {
    paint_directory_montage_overlay(painter, thumb);
  }

  // Non-recursive file count for folders (async ChildCountRole).
  if (fi != nullptr && fi->is_directory()) {
    const qint64 n = index.data(ChildCountRole).toLongLong();
    if (n >= 0) {
      paint_tile_badge(painter, thumb, QStringLiteral("%1").arg(n), Qt::AlignRight | Qt::AlignBottom);
    }
  }

  MediaKind kind = MediaKind::None;
  std::optional<filter::MediaInfo> meta;

  if (fi != nullptr && !fi->is_directory()) {
    kind = classify_extension(fi->extension());
    // Memory-only lookup on the GUI thread. Missing meta is requested async
    // for media, PDFs, and archives (pages / file_count).
    const std::string ext_l = [&] {
      std::string e = fi->extension();
      if (!e.empty() && e[0] == '.') {
        e.erase(e.begin());
      }
      for (char& c : e) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }
      return e;
    }();
    const bool want_meta =
        kind != MediaKind::None || ext_l == "pdf" || ext_l == "zip" || ext_l == "tar"
        || ext_l == "tgz" || ext_l == "7z" || ext_l == "rar" || ext_l == "cbz" || ext_l == "cbr"
        || ext_l == "jar" || ext_l == "apk" || ext_l == "gz" || ext_l == "bz2" || ext_l == "xz";
    if (want_meta) {
      auto& cache = filter::MediaMetaCache::instance();
      meta = cache.try_get(fi->path());
      if (!meta && !cache.is_negative(fi->path())) {
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

      // Duration only for video/audio and only if ≥1s (images must not show 0:00).
      if (meta && (kind == MediaKind::Video || kind == MediaKind::Audio)
          && meta->duration_ms && *meta->duration_ms >= 1000) {
        top_left = format_duration_ms(*meta->duration_ms);
      } else if (meta && meta->pages && *meta->pages > 0) {
        top_left = QStringLiteral("%1 pages").arg(*meta->pages);
      } else if (meta && meta->file_count && *meta->file_count > 0) {
        top_left = QStringLiteral("%1 files").arg(*meta->file_count);
      }
      if (meta && (kind == MediaKind::Video || kind == MediaKind::Audio)
          && meta->framerate && *meta->framerate > 0.0) {
        top_right = QStringLiteral("%1fps").arg(*meta->framerate, 0, 'g', 3);
      }
      if (meta && meta->width && meta->height && *meta->width > 0 && *meta->height > 0) {
        bottom_left = QStringLiteral("%1×%2").arg(*meta->width).arg(*meta->height);
      }

      paint_tile_badge(painter, thumb, top_left, Qt::AlignLeft | Qt::AlignTop);
      paint_tile_badge(painter, thumb, top_right, Qt::AlignRight | Qt::AlignTop);
      paint_tile_badge(painter, thumb, bottom_left, Qt::AlignLeft | Qt::AlignBottom);
    }
  }

  if (fi != nullptr) {
    paint_tag_chips(painter, thumb, fi->path());
  }

  // Type sticker after text badges; before status overlays so loading/new stay on top.
  if (fi != nullptr && !fi->is_directory()) {
    draw_type_badge(painter, thumb, kind);
  }

  paint_tile_status_overlays(painter, thumb, index);

  // Captions: basename in normal text; size/date in gray (dirtoo-py). No outline.
  if (!text.isEmpty() && model_->icon_detail_level() > 0) {
    QRect text_rect = opt.rect.adjusted(4, thumb.bottom() + 4, -4, -2);
    if (text_rect.height() > 0 && text_rect.width() > 0) {
      QPen pen = painter->pen();
      const QColor name_color = opt.palette.text().color();
      const QColor secondary(96, 96, 96);
      const QFontMetrics fm(painter->font());
      const QStringList lines = text.split(QLatin1Char('\n'));
      int y = text_rect.top();
      int drawn = 0;
      for (const QString& line : lines) {
        if (y + fm.height() > text_rect.bottom()) {
          break;
        }
        const QString elided = fm.elidedText(line, Qt::ElideMiddle, text_rect.width());
        painter->setPen(drawn == 0 ? name_color : secondary);
        painter->drawText(QRect(text_rect.left(), y, text_rect.width(), fm.height()),
                          Qt::AlignHCenter | Qt::AlignVCenter, elided);
        y += fm.height() + 1;
        ++drawn;
      }
      painter->setPen(pen);
    }
  }

  painter->restore();
}


QRect FileItemDelegate::thumb_rect_for(const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const
{
  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);
  if (index.isValid() && index.data(IsGroupStartRole).toBool()) {
    const QString label = index.data(GroupLabelRole).toString();
    if (!label.isEmpty()) {
      opt.rect.setTop(opt.rect.top() + group_header_height(option.fontMetrics));
    }
  }
  if (index.isValid()) {
    const qint64 gap = index.data(TimeGapSecondsRole).toLongLong();
    if (gap >= kTimeGapThresholdSecs) {
      opt.rect.setTop(opt.rect.top() + option.fontMetrics.height() + 6);
    }
  }
  // Detail / list: small square at left — chips are not interactive there.
  if (model_ == nullptr || !model_->icon_style_active()) {
    return {};
  }
  const int text_rows = model_->icon_text_rows();
  const int caption_budget =
      text_rows > 0 ? (6 + text_rows * 18)
                    : (model_->icon_detail_level() > 0 ? 22 : 0);
  int icon_side = opt.decorationSize.width() > 0 ? opt.decorationSize.width()
                                                 : std::min(opt.rect.width() - 8, opt.rect.height() / 2);
  icon_side = std::min(icon_side, std::max(16, opt.rect.height() - caption_budget - 8));
  icon_side = std::min(icon_side, opt.rect.width() - 8);
  QRect thumb(0, 0, icon_side, icon_side);
  thumb.moveCenter(QPoint(opt.rect.center().x(), opt.rect.top() + icon_side / 2 + 4));
  return thumb;
}

bool FileItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                   const QStyleOptionViewItem& option, const QModelIndex& index)
{
  if (event != nullptr && event->type() == QEvent::MouseButtonRelease
      && model_ != nullptr && index.isValid()) {
    const auto* me = static_cast<QMouseEvent*>(event);
    if (me->button() == Qt::LeftButton) {
      const QRect thumb = thumb_rect_for(option, index);
      if (!thumb.isEmpty()) {
        if (const auto* fi = model_->file_at(index.row());
            fi != nullptr && !fi->is_directory()) {
          const QString tag = tag_chip_at(thumb, fi->path(), me->pos());
          if (!tag.isEmpty()) {
            emit tag_chip_clicked(tag);
            return true;
          }
        }
      }
    }
  }
  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/tags/tag_store.hpp"

#include <QColor>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QRect>
#include <QString>

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dirtoo::app {
namespace tag_paint_detail {

struct TagChip {
  QString name;   // stable key
  QString label;  // display text (falls back to name)
  QColor color;
  QString badge;  // theme icon, file path, or empty
};

inline QColor color_for_tag(const dirtoo::tags::TagDef& def)
{
  if (def.color.size() >= 7 && def.color[0] == '#') {
    bool ok = false;
    const auto rgb = QString::fromStdString(def.color).mid(1).toUInt(&ok, 16);
    if (ok) {
      return QColor::fromRgb(static_cast<QRgb>(rgb | 0xff000000u));
    }
  }
  // Stable pastel from name hash.
  unsigned h = 2166136261u;
  for (unsigned char c : def.name) {
    h ^= c;
    h *= 16777619u;
  }
  return QColor::fromHsv(static_cast<int>(h % 360), 140, 220);
}

inline std::string cache_key_for_path(const std::filesystem::path& path)
{
  // Archive members store path() as the Location URL (file://…//archive:…);
  // do not absolute()-normalize those keys or lookup will miss.
  const std::string path_str = path.string();
  if (path_str.find("://") != std::string::npos || path_str.find("//archive") != std::string::npos) {
    return path_str;
  }
  std::error_code ec;
  const auto abs = std::filesystem::absolute(path, ec);
  return ec ? path_str : abs.lexically_normal().string();
}

struct ChipCacheState {
  dirtoo::hash::ChecksumStore checksums;
  dirtoo::tags::TagStore tags;
  bool open = false;
  std::once_flag once;
  std::mutex mu;
  std::unordered_map<std::string, std::vector<TagChip>> chips;
};

inline ChipCacheState& chip_cache_state()
{
  static ChipCacheState state;
  return state;
}

/// Drop cached chips after tag mutations. Empty key clears the whole cache.
inline void clear_tag_chip_cache(const std::string& path_key = {})
{
  auto& st = chip_cache_state();
  std::lock_guard<std::mutex> lock(st.mu);
  if (path_key.empty()) {
    st.chips.clear();
  } else {
    st.chips.erase(path_key);
  }
}

inline std::vector<TagChip> chips_for_path(const std::filesystem::path& path)
{
  auto& st = chip_cache_state();
  std::call_once(st.once, [&] {
    std::string err;
    st.open = st.checksums.open(dirtoo::hash::ChecksumStore::default_path(), &err)
              && st.tags.open(dirtoo::tags::TagStore::default_path(), &err);
  });

  const std::string key = cache_key_for_path(path);

  std::lock_guard<std::mutex> lock(st.mu);
  if (!st.open) {
    return {};
  }
  if (const auto it = st.chips.find(key); it != st.chips.end()) {
    return it->second;
  }
  auto digests = st.checksums.get(key);
  if (!digests) {
    st.chips.emplace(key, std::vector<TagChip>{});
    return {};
  }
  std::vector<TagChip> out;
  for (const auto& name : st.tags.tags_for_sha256(digests->sha256_hex)) {
    TagChip chip;
    chip.name = QString::fromStdString(name);
    chip.label = chip.name;
    if (auto def = st.tags.get_tag(name)) {
      chip.color = color_for_tag(*def);
      if (!def->label.empty()) {
        chip.label = QString::fromStdString(def->label);
      }
      chip.badge = QString::fromStdString(def->badge);
    } else {
      dirtoo::tags::TagDef tmp;
      tmp.name = name;
      chip.color = color_for_tag(tmp);
    }
    out.push_back(std::move(chip));
    if (out.size() >= 3) {
      break;
    }
  }
  st.chips[key] = out;
  return out;
}

} // namespace tag_paint_detail

inline QPixmap load_tag_badge_pixmap(const QString& badge, int size)
{
  if (badge.isEmpty() || size <= 0) {
    return {};
  }
  // Absolute / relative image file.
  if (QFileInfo::exists(badge)) {
    QPixmap pm(badge);
    if (!pm.isNull()) {
      return pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
  }
  // Theme icon name (e.g. "folder", "emblem-favorite").
  const QIcon ic = QIcon::fromTheme(badge);
  if (!ic.isNull()) {
    return ic.pixmap(size, size);
  }
  return {};
}

/// Draw up to three tag chips near the bottom of the thumbnail, above the
/// bottom-left meta row (Width×Height) to avoid overlap. Uses label + color
/// (+ optional badge image) from TagDef when set.
inline void paint_tag_chips(QPainter* painter, const QRect& thumb, const std::filesystem::path& path)
{
  if (painter == nullptr || thumb.isEmpty()) {
    return;
  }
  const auto chips = tag_paint_detail::chips_for_path(path);
  if (chips.empty()) {
    return;
  }
  painter->save();
  QFont font = painter->font();
  font.setPointSizeF(std::max(7.5, font.pointSizeF() * 0.8));
  painter->setFont(font);
  const QFontMetrics fm(font);
  const int pad_x = 3;
  const int h = fm.height() + 2;
  const int icon_sz = std::max(10, h - 2);
  int x = thumb.left() + 2;
  // Sit above bottom-left meta badges (e.g. Width×Height) so chips do not overlap.
  const int bottom_meta_reserve = h + 6;
  const int y = thumb.bottom() - h - 2 - bottom_meta_reserve;
  for (const auto& chip : chips) {
    QString text = chip.label.isEmpty() ? chip.name : chip.label;
    const QPixmap icon = load_tag_badge_pixmap(chip.badge, icon_sz);
    const int icon_w = icon.isNull() ? 0 : (icon_sz + 2);
    const int max_text = std::max(12, thumb.width() / 3 - icon_w);
    if (fm.horizontalAdvance(text) > max_text) {
      text = fm.elidedText(text, Qt::ElideRight, max_text);
    }
    const int w = fm.horizontalAdvance(text) + pad_x * 2 + icon_w;
    if (x + w > thumb.right() - 2) {
      break;
    }
    const QRect badge(x, y, w, h);
    painter->setPen(Qt::NoPen);
    painter->setBrush(chip.color);
    painter->drawRoundedRect(badge, 2, 2);
    int text_left = x + pad_x;
    if (!icon.isNull()) {
      const int iy = y + (h - icon_sz) / 2;
      painter->drawPixmap(text_left, iy, icon);
      text_left += icon_sz + 2;
    }
    const int lum = (chip.color.red() * 299 + chip.color.green() * 587 + chip.color.blue() * 114) / 1000;
    painter->setPen(lum > 140 ? QColor(20, 20, 20) : QColor(250, 250, 250));
    painter->drawText(QRect(text_left, y, badge.right() - text_left - pad_x + 1, h),
                      Qt::AlignVCenter | Qt::AlignLeft, text);
    x += w + 2;
  }
  painter->restore();
}

} // namespace dirtoo::app

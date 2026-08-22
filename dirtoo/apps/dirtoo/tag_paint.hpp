// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "hash_service.hpp"
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

#include <algorithm>
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
    // Checksums via process-wide HashService; only TagStore is owned here.
    st.open = HashService::instance().ensure_open(&err)
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
  auto digests = HashService::instance().get_full(key);
  if (!digests) {
    st.chips.emplace(key, std::vector<TagChip>{});
    return {};
  }
  std::vector<TagChip> out;
  for (const auto& name : st.tags.tags_for_sha256(digests->sha256_hex)) {
    TagChip chip;
    chip.name = QString::fromStdString(name);
    // Hide namespace in the chip text (show local part only). Explicit TagDef
    // label still wins when set.
    chip.label = QString::fromStdString(dirtoo::tags::local_tag_name(name));
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

/// Layout metrics shared by paint_tag_chips and tag_chip_at so hit-testing
/// cannot drift from painting. Chips stack vertically upward from above the
/// bottom-left meta row (Width×Height / duration).
struct TagChipLayout {
  QFont font;
  QFontMetrics fm{QFont{}};
  int pad_x = 3;
  int gap_y = 2;
  int h = 0;
  int icon_sz = 0;
  int max_text = 0;
  int left = 0;
  int bottom_y = 0;  // top of the bottom-most chip
  int min_top = 0;   // stop stacking above this y
};

inline TagChipLayout make_tag_chip_layout(const QRect& thumb, const QFont& base_font)
{
  TagChipLayout L;
  L.font = base_font;
  L.font.setPointSizeF(std::max(7.5, L.font.pointSizeF() * 0.8));
  L.fm = QFontMetrics(L.font);
  L.pad_x = 3;
  L.gap_y = 2;
  L.h = L.fm.height() + 2;
  L.icon_sz = std::max(10, L.h - 2);
  // Prefer wider chips than the old 1/3 width horizontal layout.
  L.max_text = std::max(24, thumb.width() - 4 - L.pad_x * 2 - L.icon_sz - 2);
  L.left = thumb.left() + 2;
  const int bottom_meta_reserve = L.h + 6;
  L.bottom_y = thumb.bottom() - L.h - 2 - bottom_meta_reserve;
  L.min_top = thumb.top() + 2;
  return L;
}

inline int chip_width_for(const TagChipLayout& L, const QString& text, bool has_icon)
{
  const int icon_w = has_icon ? (L.icon_sz + 2) : 0;
  return L.fm.horizontalAdvance(text) + L.pad_x * 2 + icon_w;
}

/// Draw tag chips stacked vertically near the bottom-left of the thumbnail,
/// above the meta row. Uses label + color (+ optional badge image) from TagDef.
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
  const TagChipLayout L = make_tag_chip_layout(thumb, painter->font());
  painter->setFont(L.font);
  int y = L.bottom_y;
  for (const auto& chip : chips) {
    if (y < L.min_top) {
      break;
    }
    QString text = chip.label.isEmpty() ? chip.name : chip.label;
    const QPixmap icon = load_tag_badge_pixmap(chip.badge, L.icon_sz);
    const bool has_icon = !icon.isNull();
    if (L.fm.horizontalAdvance(text) > L.max_text) {
      text = L.fm.elidedText(text, Qt::ElideRight, L.max_text);
    }
    const int w = std::min(chip_width_for(L, text, has_icon), thumb.right() - 2 - L.left);
    if (w <= 0) {
      break;
    }
    const QRect badge(L.left, y, w, L.h);
    painter->setPen(Qt::NoPen);
    painter->setBrush(chip.color);
    painter->drawRoundedRect(badge, 2, 2);
    int text_left = L.left + L.pad_x;
    if (has_icon) {
      const int iy = y + (L.h - L.icon_sz) / 2;
      painter->drawPixmap(text_left, iy, icon);
      text_left += L.icon_sz + 2;
    }
    const int lum = (chip.color.red() * 299 + chip.color.green() * 587 + chip.color.blue() * 114) / 1000;
    painter->setPen(lum > 140 ? QColor(20, 20, 20) : QColor(250, 250, 250));
    painter->drawText(QRect(text_left, y, badge.right() - text_left - L.pad_x + 1, L.h),
                      Qt::AlignVCenter | Qt::AlignLeft, text);
    y -= L.h + L.gap_y;
  }
  painter->restore();
}


/// Hit-test tag chips in the same layout as paint_tag_chips. Returns the stable
/// tag name (with namespace) of the chip under `pos`, or empty if none.
inline QString tag_chip_at(const QRect& thumb, const std::filesystem::path& path, const QPoint& pos)
{
  if (thumb.isEmpty() || !thumb.contains(pos)) {
    return {};
  }
  const auto chips = tag_paint_detail::chips_for_path(path);
  if (chips.empty()) {
    return {};
  }
  QFont base;
  const TagChipLayout L = make_tag_chip_layout(thumb, base);
  int y = L.bottom_y;
  for (const auto& chip : chips) {
    if (y < L.min_top) {
      break;
    }
    QString text = chip.label.isEmpty() ? chip.name : chip.label;
    const QPixmap icon = load_tag_badge_pixmap(chip.badge, L.icon_sz);
    const bool has_icon = !icon.isNull();
    if (L.fm.horizontalAdvance(text) > L.max_text) {
      text = L.fm.elidedText(text, Qt::ElideRight, L.max_text);
    }
    const int w = std::min(chip_width_for(L, text, has_icon), thumb.right() - 2 - L.left);
    if (w <= 0) {
      break;
    }
    const QRect badge(L.left, y, w, L.h);
    if (badge.contains(pos)) {
      return chip.name;
    }
    y -= L.h + L.gap_y;
  }
  return {};
}

} // namespace dirtoo::app

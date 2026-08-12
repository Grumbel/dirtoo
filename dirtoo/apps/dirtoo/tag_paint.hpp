// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/tags/tag_store.hpp"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QRect>
#include <QString>

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace dirtoo::app {
namespace tag_paint_detail {

struct TagChip {
  QString name;
  QColor color;
};

inline QColor color_for_tag(const dirtoo::tags::TagDef& def)
{
  if (def.color.size() >= 7 && def.color[0] == '#') {
    bool ok = false;
    const auto rgb = QString::fromStdString(def.color).mid(1).toUInt(&ok, 16);
    if (ok) {
      return QColor.fromRgb(static_cast<QRgb>(rgb | 0xff000000u));
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

inline std::vector<TagChip> chips_for_path(const std::filesystem::path& path)
{
  static dirtoo::hash::ChecksumStore checksums;
  static dirtoo::tags::TagStore tags;
  static bool open = false;
  static std::once_flag once;
  static std::mutex mu;

  std::call_once(once, [] {
    std::string err;
    open = checksums.open(dirtoo::hash::ChecksumStore::default_path(), &err)
           && tags.open(dirtoo::tags::TagStore::default_path(), &err);
  });

  std::lock_guard<std::mutex> lock(mu);
  if (!open) {
    return {};
  }
  std::error_code ec;
  const auto abs = std::filesystem::absolute(path, ec);
  const std::string key = ec ? path.string() : abs.lexically_normal().string();
  auto digests = checksums.get(key);
  if (!digests) {
    return {};
  }
  std::vector<TagChip> out;
  for (const auto& name : tags.tags_for_sha256(digests->sha256_hex)) {
    TagChip chip;
    chip.name = QString::fromStdString(name);
    if (auto def = tags.get_tag(name)) {
      chip.color = color_for_tag(*def);
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
  return out;
}

} // namespace tag_paint_detail

/// Draw up to three tag chips along the bottom edge of the thumbnail.
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
  int x = thumb.left() + 2;
  const int y = thumb.bottom() - h - 2;
  for (const auto& chip : chips) {
    QString text = chip.name;
    if (fm.horizontalAdvance(text) > thumb.width() / 3) {
      text = fm.elidedText(text, Qt::ElideRight, thumb.width() / 3);
    }
    const int w = fm.horizontalAdvance(text) + pad_x * 2;
    if (x + w > thumb.right() - 2) {
      break;
    }
    const QRect badge(x, y, w, h);
    painter->setPen(Qt::NoPen);
    painter->setBrush(chip.color);
    painter->drawRoundedRect(badge, 2, 2);
    // Contrast text
    const int lum = (chip.color.red() * 299 + chip.color.green() * 587 + chip.color.blue() * 114) / 1000;
    painter->setPen(lum > 140 ? QColor(20, 20, 20) : QColor(250, 250, 250));
    painter->drawText(badge.adjusted(pad_x, 0, -pad_x, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    x += w + 2;
  }
  painter->restore();
}

} // namespace dirtoo::app

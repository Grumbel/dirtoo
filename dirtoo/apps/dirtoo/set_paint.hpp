// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/sets/file_set_store.hpp"

#include <QColor>
#include <QPainter>
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
namespace set_paint_detail {

inline std::string path_key(const std::filesystem::path& path)
{
  const std::string path_str = path.string();
  if (path_str.find("://") != std::string::npos || path_str.find("//archive") != std::string::npos) {
    return path_str;
  }
  std::error_code ec;
  const auto abs = std::filesystem::absolute(path, ec);
  if (ec) {
    return path_str;
  }
  return abs.lexically_normal().string();
}

inline QColor color_for_set(const dirtoo::sets::FileSet& s)
{
  if (s.color.size() >= 7 && s.color[0] == '#') {
    bool ok = false;
    const auto rgb = QString::fromStdString(s.color).mid(1).toUInt(&ok, 16);
    if (ok) {
      return QColor::fromRgb(static_cast<QRgb>(rgb | 0xff000000u));
    }
  }
  unsigned h = 2166136261u;
  for (unsigned char c : s.id) {
    h ^= c;
    h *= 16777619u;
  }
  // Distinct from tag pastels: slightly higher saturation.
  return QColor::fromHsv(static_cast<int>(h % 360), 180, 210);
}

inline dirtoo::sets::FileSetStore& shared_store()
{
  static dirtoo::sets::FileSetStore store;
  static std::once_flag once;
  std::call_once(once, [] {
    std::string err;
    (void)store.open(dirtoo::sets::FileSetStore::default_path(), &err);
  });
  return store;
}

inline std::vector<dirtoo::sets::FileSet> sets_for_path(const std::filesystem::path& path)
{
  static std::mutex mu;
  static std::unordered_map<std::string, std::vector<dirtoo::sets::FileSet>> cache;
  const std::string key = path_key(path);
  std::lock_guard lock(mu);
  if (auto it = cache.find(key); it != cache.end()) {
    return it->second;
  }
  auto& store = shared_store();
  if (!store.is_open()) {
    return {};
  }
  auto sets = store.sets_for_path(key);
  cache[key] = sets;
  return sets;
}

} // namespace set_paint_detail

/// Left-edge stripes for set membership (one stripe per set, max 4).
inline void paint_set_membership(QPainter* painter, const QRect& tile,
                                 const std::filesystem::path& path)
{
  if (painter == nullptr || tile.isEmpty()) {
    return;
  }
  const auto sets = set_paint_detail::sets_for_path(path);
  if (sets.empty()) {
    return;
  }
  constexpr int kMax = 4;
  constexpr int kStripe = 3;
  constexpr int kGap = 1;
  int x = tile.left() + 1;
  const int n = std::min(static_cast<int>(sets.size()), kMax);
  for (int i = 0; i < n; ++i) {
    painter->fillRect(QRect(x, tile.top() + 2, kStripe, tile.height() - 4),
                      set_paint_detail::color_for_set(sets[static_cast<std::size_t>(i)]));
    x += kStripe + kGap;
  }
}

} // namespace dirtoo::app

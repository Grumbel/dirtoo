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

inline std::mutex& cache_mu()
{
  static std::mutex mu;
  return mu;
}

inline std::unordered_map<std::string, std::vector<dirtoo::sets::FileSet>>& cache_map()
{
  static std::unordered_map<std::string, std::vector<dirtoo::sets::FileSet>> cache;
  return cache;
}

inline void clear_set_membership_cache()
{
  std::lock_guard lock(cache_mu());
  cache_map().clear();
}

inline std::vector<dirtoo::sets::FileSet> sets_for_path(const std::filesystem::path& path)
{
  const std::string key = path_key(path);
  std::lock_guard lock(cache_mu());
  if (auto it = cache_map().find(key); it != cache_map().end()) {
    return it->second;
  }
  auto& store = shared_store();
  if (!store.is_open()) {
    return {};
  }
  auto sets = store.sets_for_path(key);
  cache_map()[key] = sets;
  return sets;
}

} // namespace set_paint_detail

/// Invalidate path→sets cache after create/update membership (paint sees new data).
inline void clear_set_membership_cache()
{
  set_paint_detail::clear_set_membership_cache();
}

/// Top-edge color bars for set membership (one bar per set, stacked, max 4).
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
  constexpr int kBar = 3;
  constexpr int kGap = 1;
  int y = tile.top() + 1;
  const int n = std::min(static_cast<int>(sets.size()), kMax);
  const int x = tile.left() + 1;
  const int w = std::max(1, tile.width() - 2);
  for (int i = 0; i < n; ++i) {
    painter->fillRect(QRect(x, y, w, kBar),
                      set_paint_detail::color_for_set(sets[static_cast<std::size_t>(i)]));
    y += kBar + kGap;
  }
}

} // namespace dirtoo::app

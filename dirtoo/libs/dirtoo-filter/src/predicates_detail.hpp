// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Internal helpers shared by predicates_*.cpp translation units.

#include "dirtoo/filter/media_meta_cache.hpp"
#include "dirtoo/filter/media_probe.hpp"

#include <filesystem>
#include <optional>

namespace dirtoo::filter::detail {

[[nodiscard]] inline std::optional<MediaInfo> lookup_media(const std::filesystem::path& path)
{
  auto& cache = MediaMetaCache::instance();
  if (auto hit = cache.try_get(path)) {
    return hit;
  }
  if (cache.is_negative(path)) {
    return std::nullopt;
  }
  // CLI / non-GUI: resolve synchronously via workers+SQLite (not for paint).
  return resolve_media_cached(path);
}

} // namespace dirtoo::filter::detail

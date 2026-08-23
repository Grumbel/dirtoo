// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/collection/sorter.hpp"

#include "dirtoo/filter/media_meta_cache.hpp"

#include <algorithm>
#include <cctype>
#include <random>
#include <string_view>

namespace dirtoo::collection {
namespace {

std::string to_lower(std::string s)
{
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

int cmp_natural(const std::vector<NaturalPiece>& a, const std::vector<NaturalPiece>& b)
{
  const std::size_t n = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (a[i].is_number != b[i].is_number) {
      // numbers before pure text at same position — keep tuple parity with Python
      // by comparing as string vs number never happens if split is consistent
      if (a[i].is_number) {
        return -1;
      }
      return 1;
    }
    if (a[i].is_number) {
      if (a[i].number < b[i].number) {
        return -1;
      }
      if (a[i].number > b[i].number) {
        return 1;
      }
    } else {
      if (a[i].text < b[i].text) {
        return -1;
      }
      if (a[i].text > b[i].text) {
        return 1;
      }
    }
  }
  if (a.size() < b.size()) {
    return -1;
  }
  if (a.size() > b.size()) {
    return 1;
  }
  return 0;
}

struct MediaDims {
  std::uint32_t w = 0;
  std::uint32_t h = 0;
  std::uint64_t duration_ms = 0;
  double fps = 0;
  bool ok = false;
};

MediaDims media_of(const fs::FileInfo& fi)
{
  MediaDims d;
  if (fi.is_directory() || fi.path().empty()) {
    return d;
  }
  // GUI/sort path: memory cache only — never ffprobe/SQLite here.
  const auto meta = filter::MediaMetaCache::instance().try_get(fi.path());
  if (!meta) {
    return d;
  }
  d.ok = true;
  d.w = meta->width.value_or(0);
  d.h = meta->height.value_or(0);
  d.duration_ms = meta->duration_ms.value_or(0);
  d.fps = meta->framerate.value_or(0.0);
  return d;
}

} // namespace

std::vector<NaturalPiece> numeric_sort_key(std::string_view text)
{
  std::vector<NaturalPiece> out;
  std::size_t i = 0;
  while (i < text.size()) {
    if (std::isdigit(static_cast<unsigned char>(text[i]))) {
      NaturalPiece p;
      p.is_number = true;
      std::uint64_t n = 0;
      while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
        n = n * 10 + static_cast<std::uint64_t>(text[i] - '0');
        ++i;
      }
      p.number = n;
      out.push_back(std::move(p));
    } else {
      NaturalPiece p;
      p.is_number = false;
      while (i < text.size() && !std::isdigit(static_cast<unsigned char>(text[i]))) {
        p.text.push_back(text[i]);
        ++i;
      }
      out.push_back(std::move(p));
    }
  }
  return out;
}

int Sorter::compare(const fs::FileInfo& a, const fs::FileInfo& b) const
{
  if (directories_first_) {
    if (a.is_directory() != b.is_directory()) {
      return a.is_directory() ? -1 : 1;
    }
  }

  auto by_name = [](const fs::FileInfo& x, const fs::FileInfo& y) {
    return cmp_natural(numeric_sort_key(to_lower(x.basename())),
                       numeric_sort_key(to_lower(y.basename())));
  };

  switch (key_) {
  case SortKey::Name:
    return by_name(a, b);
  case SortKey::Size: {
    if (a.size() < b.size()) {
      return -1;
    }
    if (a.size() > b.size()) {
      return 1;
    }
    return by_name(a, b);
  }
  case SortKey::Extension: {
    const auto ea = to_lower(a.extension());
    const auto eb = to_lower(b.extension());
    if (ea < eb) {
      return -1;
    }
    if (ea > eb) {
      return 1;
    }
    return by_name(a, b);
  }
  case SortKey::Modified: {
    if (a.mtime() < b.mtime()) {
      return -1;
    }
    if (a.mtime() > b.mtime()) {
      return 1;
    }
    return by_name(a, b);
  }
  case SortKey::Type: {
    // Same as extension for files; directories first already handled
    const auto ea = to_lower(a.extension());
    const auto eb = to_lower(b.extension());
    if (ea < eb) {
      return -1;
    }
    if (ea > eb) {
      return 1;
    }
    return by_name(a, b);
  }
  case SortKey::Width: {
    const auto ma = media_of(a);
    const auto mb = media_of(b);
    if (ma.w < mb.w) {
      return -1;
    }
    if (ma.w > mb.w) {
      return 1;
    }
    return by_name(a, b);
  }
  case SortKey::Height: {
    const auto ma = media_of(a);
    const auto mb = media_of(b);
    if (ma.h < mb.h) {
      return -1;
    }
    if (ma.h > mb.h) {
      return 1;
    }
    return by_name(a, b);
  }
  case SortKey::Resolution: {
    const auto ma = media_of(a);
    const auto mb = media_of(b);
    const auto ra = static_cast<std::uint64_t>(ma.w) * ma.h;
    const auto rb = static_cast<std::uint64_t>(mb.w) * mb.h;
    if (ra < rb) {
      return -1;
    }
    if (ra > rb) {
      return 1;
    }
    return by_name(a, b);
  }
  case SortKey::AspectRatio: {
    const auto ma = media_of(a);
    const auto mb = media_of(b);
    const double aa = ma.h == 0 ? 0.0 : static_cast<double>(ma.w) / static_cast<double>(ma.h);
    const double ab = mb.h == 0 ? 0.0 : static_cast<double>(mb.w) / static_cast<double>(mb.h);
    if (aa < ab) {
      return -1;
    }
    if (aa > ab) {
      return 1;
    }
    return by_name(a, b);
  }
  case SortKey::Duration: {
    const auto ma = media_of(a);
    const auto mb = media_of(b);
    if (ma.duration_ms < mb.duration_ms) {
      return -1;
    }
    if (ma.duration_ms > mb.duration_ms) {
      return 1;
    }
    return by_name(a, b);
  }
  case SortKey::Framerate: {
    const auto ma = media_of(a);
    const auto mb = media_of(b);
    if (ma.fps < mb.fps) {
      return -1;
    }
    if (ma.fps > mb.fps) {
      return 1;
    }
    return by_name(a, b);
  }
  case SortKey::Permissions: {
    using Per = std::filesystem::perms;
    const auto pa = static_cast<unsigned>(a.permissions() & Per::mask);
    const auto pb = static_cast<unsigned>(b.permissions() & Per::mask);
    if (pa < pb) {
      return -1;
    }
    if (pa > pb) {
      return 1;
    }
    return by_name(a, b);
  }
  case SortKey::Random:
    return 0; // handled in sort()
  }
  return by_name(a, b);
}

void Sorter::sort(std::vector<fs::FileInfo>& items) const
{
  if (key_ == SortKey::Random) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::shuffle(items.begin(), items.end(), rng);
    if (directories_first_) {
      std::stable_partition(items.begin(), items.end(),
                            [](const fs::FileInfo& fi) { return fi.is_directory(); });
    }
    return;
  }

  std::stable_sort(items.begin(), items.end(), [this](const fs::FileInfo& a, const fs::FileInfo& b) {
    const int c = compare(a, b);
    if (ascending_) {
      return c < 0;
    }
    // Reverse payload but keep directories first if enabled
    if (directories_first_ && a.is_directory() != b.is_directory()) {
      return a.is_directory();
    }
    return c > 0;
  });
}

} // namespace dirtoo::collection

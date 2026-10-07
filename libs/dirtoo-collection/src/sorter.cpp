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
      // Compare as digit strings (no leading zeros): longer = bigger, then
      // lexicographic. A uint64 would silently wrap for 20+ digit runs.
      if (a[i].digits.size() != b[i].digits.size()) {
        return a[i].digits.size() < b[i].digits.size() ? -1 : 1;
      }
      if (const int c = a[i].digits.compare(b[i].digits); c != 0) {
        return c < 0 ? -1 : 1;
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
      bool saturated = false;
      const std::size_t begin = i;
      while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
        const auto d = static_cast<std::uint64_t>(text[i] - '0');
        if (!saturated) {
          if (n > (UINT64_MAX - d) / 10) {
            saturated = true;
            n = UINT64_MAX;
          } else {
            n = n * 10 + d;
          }
        }
        ++i;
      }
      p.text.assign(text.substr(begin, i - begin));
      std::size_t nz = 0;
      while (nz + 1 < p.text.size() && p.text[nz] == '0') {
        ++nz;
      }
      p.digits = p.text.substr(nz);
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

namespace {

/// Everything that is expensive to derive from a FileInfo and needed on every
/// comparison, computed once per item per sort (n instead of n·log n times).
struct SortKeys {
  std::vector<NaturalPiece> name;  ///< natural key of the lower-cased basename
  std::string ext;                 ///< lower-cased extension
  MediaDims media;                 ///< only filled for media sort keys
};

bool is_media_key(SortKey k)
{
  switch (k) {
  case SortKey::Width:
  case SortKey::Height:
  case SortKey::Resolution:
  case SortKey::AspectRatio:
  case SortKey::Duration:
  case SortKey::Framerate:
    return true;
  default:
    return false;
  }
}

SortKeys make_keys(const fs::FileInfo& fi, SortKey key)
{
  SortKeys k;
  k.name = numeric_sort_key(to_lower(fi.basename()));
  if (key == SortKey::Extension || key == SortKey::Type) {
    k.ext = to_lower(fi.extension());
  }
  if (is_media_key(key)) {
    k.media = media_of(fi);
  }
  return k;
}

template <typename T>
int three_way(const T& a, const T& b)
{
  return a < b ? -1 : (b < a ? 1 : 0);
}

} // namespace

namespace {

int compare_keyed(SortKey key, bool directories_first, const fs::FileInfo& a,
                  const fs::FileInfo& b, const SortKeys& ka, const SortKeys& kb)
{
  if (directories_first && a.is_directory() != b.is_directory()) {
    return a.is_directory() ? -1 : 1;
  }
  const auto by_name = [&] { return cmp_natural(ka.name, kb.name); };
  const auto then_name = [&](int c) { return c != 0 ? c : by_name(); };

  switch (key) {
  case SortKey::Name:
    return by_name();
  case SortKey::Size:
    return then_name(three_way(a.size(), b.size()));
  case SortKey::Extension:
  case SortKey::Type:  // same ordering as Extension for now (REVIEW G4)
    return then_name(three_way(ka.ext, kb.ext));
  case SortKey::Modified:
    // Entries without a time (archive members, ...) sort before every real
    // time instead of after all of them as a year-2174 "newest".
    return then_name(three_way(a.mtime_unix_ns().value_or(INT64_MIN),
                               b.mtime_unix_ns().value_or(INT64_MIN)));
  case SortKey::Width:
    return then_name(three_way(ka.media.w, kb.media.w));
  case SortKey::Height:
    return then_name(three_way(ka.media.h, kb.media.h));
  case SortKey::Resolution:
    return then_name(three_way(static_cast<std::uint64_t>(ka.media.w) * ka.media.h,
                               static_cast<std::uint64_t>(kb.media.w) * kb.media.h));
  case SortKey::AspectRatio: {
    const double aa = ka.media.h == 0 ? 0.0 : static_cast<double>(ka.media.w) / ka.media.h;
    const double ab = kb.media.h == 0 ? 0.0 : static_cast<double>(kb.media.w) / kb.media.h;
    return then_name(three_way(aa, ab));
  }
  case SortKey::Duration:
    return then_name(three_way(ka.media.duration_ms, kb.media.duration_ms));
  case SortKey::Framerate:
    return then_name(three_way(ka.media.fps, kb.media.fps));
  case SortKey::Permissions: {
    using Per = std::filesystem::perms;
    return then_name(three_way(static_cast<unsigned>(a.permissions() & Per::mask),
                               static_cast<unsigned>(b.permissions() & Per::mask)));
  }
  case SortKey::Random:
    return 0; // handled in sort()
  }
  return by_name();
}

} // namespace

int Sorter::compare(const fs::FileInfo& a, const fs::FileInfo& b) const
{
  return compare_keyed(key_, directories_first_, a, b, make_keys(a, key_), make_keys(b, key_));
}

void Sorter::reshuffle()
{
  random_seed_ = std::random_device{}();
}

void Sorter::sort(std::vector<fs::FileInfo>& items) const
{
  if (key_ == SortKey::Random) {
    // Order by a seeded hash of the path (splitmix64 finaliser): stable across
    // rebuilds and independent of the current order, unlike a std::shuffle.
    auto rank = [seed = random_seed_](const fs::FileInfo& fi) {
      std::uint64_t x = std::hash<std::string>{}(fi.path().string()) ^ (std::uint64_t{seed} << 32 | seed);
      x += 0x9e3779b97f4a7c15ull;
      x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
      x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
      return x ^ (x >> 31);
    };
    std::stable_sort(items.begin(), items.end(),
                     [&](const fs::FileInfo& a, const fs::FileInfo& b) {
                       if (directories_first_ && a.is_directory() != b.is_directory()) {
                         return a.is_directory();
                       }
                       return rank(a) < rank(b);
                     });
    return;
  }

  // Decorate–sort–undecorate: derive the expensive keys once per item, sort
  // an index permutation, then move the items into place.
  const std::size_t n = items.size();
  std::vector<SortKeys> keys;
  keys.reserve(n);
  for (const auto& fi : items) {
    keys.push_back(make_keys(fi, key_));
  }
  std::vector<std::size_t> order(n);
  for (std::size_t i = 0; i < n; ++i) {
    order[i] = i;
  }
  std::stable_sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) {
    const int c = compare_keyed(key_, directories_first_, items[x], items[y], keys[x], keys[y]);
    if (ascending_) {
      return c < 0;
    }
    // Reverse payload but keep directories first if enabled
    if (directories_first_ && items[x].is_directory() != items[y].is_directory()) {
      return items[x].is_directory();
    }
    return c > 0;
  });
  std::vector<fs::FileInfo> sorted;
  sorted.reserve(n);
  for (const std::size_t i : order) {
    sorted.push_back(std::move(items[i]));
  }
  items = std::move(sorted);
}

} // namespace dirtoo::collection

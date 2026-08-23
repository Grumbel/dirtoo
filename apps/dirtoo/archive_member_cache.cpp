// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "archive_member_cache.hpp"

#include "dirtoo/archive/archive_index.hpp"

#include <QStandardPaths>
#include <QString>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace dirtoo::app {
namespace {

std::uint64_t directory_size_bytes(const std::filesystem::path& root)
{
  std::uint64_t total = 0;
  std::error_code ec;
  const auto opts = std::filesystem::directory_options::skip_permission_denied;
  for (std::filesystem::recursive_directory_iterator it(root, opts, ec), end;
       !ec && it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (it->is_regular_file(ec)) {
      total += static_cast<std::uint64_t>(it->file_size(ec));
      if (ec) {
        ec.clear();
      }
    }
  }
  return total;
}

struct TreeInfo {
  std::filesystem::path path;
  std::filesystem::file_time_type mtime{};
  std::uint64_t size_bytes = 0;
};

std::vector<TreeInfo> list_extract_trees(const std::filesystem::path& cache_root)
{
  std::vector<TreeInfo> trees;
  std::error_code ec;
  if (!std::filesystem::is_directory(cache_root, ec)) {
    return trees;
  }
  const auto opts = std::filesystem::directory_options::skip_permission_denied;
  for (const auto& entry : std::filesystem::directory_iterator(cache_root, opts, ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (!entry.is_directory(ec)) {
      continue;
    }
    TreeInfo info;
    info.path = entry.path();
    info.mtime = entry.last_write_time(ec);
    if (ec) {
      ec.clear();
      info.mtime = {};
    }
    info.size_bytes = directory_size_bytes(info.path);
    trees.push_back(std::move(info));
  }
  return trees;
}

} // namespace

std::filesystem::path archive_member_cache_root(std::string_view subdir)
{
  const QString base =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
      + QLatin1Char('/')
      + QString::fromUtf8(subdir.data(), static_cast<int>(subdir.size()));
  return std::filesystem::path{base.toStdString()};
}

std::filesystem::path archive_member_dest_dir(const std::filesystem::path& cache_root,
                                              const std::filesystem::path& archive_file)
{
  return cache_root / std::to_string(std::hash<std::string>{}(archive_file.string()));
}

std::optional<std::filesystem::path>
ensure_archive_member_extracted(const std::filesystem::path& archive_file,
                                const std::filesystem::path& member,
                                const std::filesystem::path& dest_dir)
{
  const auto cached = dest_dir / member;
  std::error_code ec;
  if (std::filesystem::is_regular_file(cached, ec) && !ec) {
    return cached;
  }
  auto extracted = archive::extract_member(archive_file, member, dest_dir);
  if (extracted) {
    return *extracted;
  }
  return std::nullopt;
}

ArchiveCachePruneStats prune_archive_member_cache(const std::filesystem::path& cache_root,
                                                  const ArchiveCachePruneOptions& options)
{
  ArchiveCachePruneStats stats;
  std::error_code ec;
  if (!std::filesystem::exists(cache_root, ec) || ec) {
    return stats;
  }

  auto trees = list_extract_trees(cache_root);
  if (trees.empty()) {
    // Also remove loose files left at the root (legacy layout).
    const auto opts = std::filesystem::directory_options::skip_permission_denied;
    for (const auto& entry : std::filesystem::directory_iterator(cache_root, opts, ec)) {
      if (ec) {
        ec.clear();
        continue;
      }
      if (entry.is_regular_file(ec)) {
        const auto sz = static_cast<std::uint64_t>(entry.file_size(ec));
        std::filesystem::remove(entry.path(), ec);
        if (!ec) {
          stats.trees_removed += 1;
          stats.bytes_removed += sz;
        }
        ec.clear();
      }
    }
    return stats;
  }

  const auto now = std::filesystem::file_time_type::clock::now();
  std::vector<TreeInfo> survivors;
  survivors.reserve(trees.size());

  for (auto& tree : trees) {
    bool remove = false;
    if (options.max_age_seconds > 0 && tree.mtime.time_since_epoch().count() != 0) {
      const auto age = std::chrono::duration_cast<std::chrono::seconds>(now - tree.mtime);
      if (age.count() >= 0
          && static_cast<std::uint64_t>(age.count()) >= options.max_age_seconds) {
        remove = true;
      }
    }
    if (remove) {
      const auto sz = tree.size_bytes;
      std::filesystem::remove_all(tree.path, ec);
      if (!ec) {
        stats.trees_removed += 1;
        stats.bytes_removed += sz;
      }
      ec.clear();
    } else {
      survivors.push_back(std::move(tree));
    }
  }

  if (options.max_total_bytes > 0) {
    std::uint64_t total = 0;
    for (const auto& t : survivors) {
      total += t.size_bytes;
    }
    if (total > options.max_total_bytes) {
      std::sort(survivors.begin(), survivors.end(),
                [](const TreeInfo& a, const TreeInfo& b) { return a.mtime < b.mtime; });
      for (auto& tree : survivors) {
        if (total <= options.max_total_bytes) {
          break;
        }
        const auto sz = tree.size_bytes;
        std::filesystem::remove_all(tree.path, ec);
        if (!ec) {
          stats.trees_removed += 1;
          stats.bytes_removed += sz;
          total -= sz;
          tree.size_bytes = 0;
          tree.path.clear();
        }
        ec.clear();
      }
      survivors.erase(std::remove_if(survivors.begin(), survivors.end(),
                                     [](const TreeInfo& t) { return t.path.empty(); }),
                      survivors.end());
    }
  }

  for (const auto& t : survivors) {
    stats.trees_remaining += 1;
    stats.bytes_remaining += t.size_bytes;
  }
  return stats;
}

ArchiveCachePruneStats prune_all_archive_member_caches(const ArchiveCachePruneOptions& options)
{
  ArchiveCachePruneStats total;
  const char* subdirs[] = {"dirtoo-open", "dirtoo-archive-thumbs", "dirtoo-archive-drop"};
  for (const char* sub : subdirs) {
    const auto s = prune_archive_member_cache(archive_member_cache_root(sub), options);
    total.trees_removed += s.trees_removed;
    total.bytes_removed += s.bytes_removed;
    total.bytes_remaining += s.bytes_remaining;
    total.trees_remaining += s.trees_remaining;
  }
  // Legacy open path under system temp (pre-cache-location layout).
  std::error_code ec;
  const auto legacy = std::filesystem::temp_directory_path(ec) / "dirtoo-open";
  if (!ec) {
    const auto s = prune_archive_member_cache(legacy, options);
    total.trees_removed += s.trees_removed;
    total.bytes_removed += s.bytes_removed;
    total.bytes_remaining += s.bytes_remaining;
    total.trees_remaining += s.trees_remaining;
  }
  return total;
}

} // namespace dirtoo::app

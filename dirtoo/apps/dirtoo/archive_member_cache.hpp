// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace dirtoo::app {

/// Cache root for extracted archive members used by drag-out and thumbnails.
[[nodiscard]] std::filesystem::path archive_member_cache_root(std::string_view subdir);

/// Deterministic extract directory for one archive file under @p cache_root.
[[nodiscard]] std::filesystem::path archive_member_dest_dir(
    const std::filesystem::path& cache_root,
    const std::filesystem::path& archive_file);

/// Return an existing extracted member path, or extract via libarchive.
/// Empty optional on failure.
[[nodiscard]] std::optional<std::filesystem::path>
ensure_archive_member_extracted(const std::filesystem::path& archive_file,
                                const std::filesystem::path& member,
                                const std::filesystem::path& dest_dir);

struct ArchiveCachePruneOptions {
  /// Delete top-level extract trees older than this (mtime of the tree root).
  /// Default 7 days. 0 disables age-based pruning.
  std::uint64_t max_age_seconds = 7ULL * 24 * 60 * 60;
  /// If total size exceeds this, delete oldest trees until under budget.
  /// Default 2 GiB. 0 disables size-based pruning.
  std::uint64_t max_total_bytes = 2ULL << 30;
};

struct ArchiveCachePruneStats {
  std::uint64_t trees_removed = 0;
  std::uint64_t bytes_removed = 0;
  std::uint64_t bytes_remaining = 0;
  std::uint64_t trees_remaining = 0;
};

/// Prune extract trees under @p cache_root (each child of the root is one archive).
/// Safe no-op if the path does not exist. Never throws.
[[nodiscard]] ArchiveCachePruneStats
prune_archive_member_cache(const std::filesystem::path& cache_root,
                           const ArchiveCachePruneOptions& options = {});

/// Prune known extract caches (open / thumbs / drop) and legacy /tmp/dirtoo-open.
/// Intended once at process start.
[[nodiscard]] ArchiveCachePruneStats prune_all_archive_member_caches(
    const ArchiveCachePruneOptions& options = {});

} // namespace dirtoo::app

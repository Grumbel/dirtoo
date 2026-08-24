// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/tree/fs_tree_node.hpp"
#include "dirtoo/tree/scan_tree.hpp"

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace dirtoo::tree {

/// In-memory hierarchical filesystem tree cache (process-wide style usage).
///
/// Thread-safe snapshot/invalidate. Scans are synchronous on the caller thread
/// (run from a worker thread in the GUI). No SQLite in phase 1 — durable tier
/// comes later without changing this API surface much.
class FsTreeCache {
public:
  FsTreeCache() = default;
  FsTreeCache(const FsTreeCache&) = delete;
  FsTreeCache& operator=(const FsTreeCache&) = delete;

  /// Canonicalize for map keys when possible; falls back to weakly path string.
  [[nodiscard]] static std::string path_key(const std::filesystem::path& path);

  /// Return cached root snapshot if present (may be Partial).
  [[nodiscard]] std::shared_ptr<const FsTreeNode>
  snapshot(const std::filesystem::path& root) const;

  /// Scan (or rescan) @p root and store the result. Returns the new snapshot.
  /// @p cancel is checked during the walk.
  std::shared_ptr<const FsTreeNode>
  scan(const std::filesystem::path& root, const ScanOptions& options = {},
       const std::atomic_bool* cancel = nullptr, ScanProgressFn progress = {});

  /// Drop one root (exact key).
  void invalidate(const std::filesystem::path& root);

  /// Drop every cached root whose path is @p root or under it.
  void invalidate_subtree(const std::filesystem::path& root);

  void clear();

  [[nodiscard]] std::size_t size() const;

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<const FsTreeNode>> roots_;
};

} // namespace dirtoo::tree

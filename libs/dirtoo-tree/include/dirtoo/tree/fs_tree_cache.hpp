// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/tree/fs_tree_node.hpp"
#include "dirtoo/tree/scan_tree.hpp"
#include "dirtoo/tree/fs_tree_size_store.hpp"

#include <atomic>
#include <cstdint>
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
/// (run from a worker thread in the GUI). Holds full root snapshots and a
/// progressive path→total_size index filled as nodes complete mid-scan.
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

  /// Progressive size for any path observed during a scan (files or directories).
  [[nodiscard]] std::optional<std::uint64_t>
  entry_total_size(const std::filesystem::path& path) const;

  /// Record a finalized node size (called from scan node_ready / tests).
  void set_entry_total_size(const std::filesystem::path& path, std::uint64_t total_size);

  /// Scan (or rescan) @p root and store the result. Publishes entry sizes as
  /// nodes complete when callbacks include node_ready (wired by default).
  std::shared_ptr<const FsTreeNode>
  scan(const std::filesystem::path& root, const ScanOptions& options = {},
       const std::atomic_bool* cancel = nullptr, ScanCallbacks callbacks = {});

  /// Drop one root snapshot and size-index entries under that path.
  void invalidate(const std::filesystem::path& root);

  /// Drop every cached root whose path is @p root or under it; prune size index.
  void invalidate_subtree(const std::filesystem::path& root);

  void clear();

  [[nodiscard]] std::size_t size() const;

  /// Optional durable size index (not owned). load_persisted() fills entry sizes.
  void set_size_store(FsTreeSizeStore* store);
  void load_persisted_sizes();

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<const FsTreeNode>> roots_;
  std::unordered_map<std::string, std::uint64_t> entry_sizes_;
  FsTreeSizeStore* size_store_ = nullptr; // non-owning
};

} // namespace dirtoo::tree

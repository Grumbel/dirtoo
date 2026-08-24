// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/tree/fs_tree_node.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

namespace dirtoo::tree {

struct ScanOptions {
  /// Maximum directory depth from the root (0 = root only, nullopt = unlimited).
  std::optional<std::uint32_t> max_depth{};
  /// Follow directory symlinks (default false — avoids cycles / surprise mounts).
  bool follow_symlinks = false;
  /// Descend into directories on a different st_dev than the root (default false).
  bool cross_device = false;
  /// Include dot entries (default true; matches dirtoo listing).
  bool include_hidden = true;
};

/// Optional progress callback: nodes visited so far, current path.
using ScanProgressFn = std::function<void(std::uint64_t nodes_seen, const std::filesystem::path& current)>;

/// Recursively scan @p root into an FsTreeNode tree (Qt-free).
///
/// Uses a breadth-first walk so shallow levels complete first (partial snapshots
/// are useful early). Aggregates `total_size` bottom-up when children finish.
///
/// Cancellation: if @p cancel is non-null and becomes true, the scan stops and
/// unfinished directories are marked Partial or Failed as appropriate.
///
/// Errors opening the root yield a single Failed node (never null).
[[nodiscard]] std::shared_ptr<const FsTreeNode>
scan_tree(const std::filesystem::path& root, const ScanOptions& options = {},
          const std::atomic_bool* cancel = nullptr, ScanProgressFn progress = {});

} // namespace dirtoo::tree

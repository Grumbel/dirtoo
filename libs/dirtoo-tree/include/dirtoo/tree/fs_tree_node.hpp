// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace dirtoo::tree {

enum class FsTreeNodeKind : std::uint8_t {
  File = 0,
  Directory,
  Symlink,
  Other,
};

enum class FsTreeNodeState : std::uint8_t {
  /// Not yet scanned (placeholder).
  Pending = 0,
  /// Some descendants known; totals may still grow.
  Partial,
  /// Subtree scan finished successfully (for this node).
  Complete,
  /// Scan failed for this node (permissions, I/O, cancelled mid-node).
  Failed,
};

/// Hierarchical filesystem node (structure + cheap lstat-class metadata).
///
/// Files and symlinks are leaves. Directories hold children. `total_size` is
/// `own_size` plus the sum of children's `total_size` when the subtree is
/// Complete; under Partial it is a lower bound of what has been seen so far.
///
/// Published snapshots should be treated as immutable: share via
/// `shared_ptr<const FsTreeNode>`.
class FsTreeNode {
public:
  FsTreeNode() = default;

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  [[nodiscard]] FsTreeNodeKind kind() const noexcept { return kind_; }
  [[nodiscard]] FsTreeNodeState state() const noexcept { return state_; }

  /// Byte size of this node itself (regular file st_size; typically 0 for dirs).
  [[nodiscard]] std::uint64_t own_size() const noexcept { return own_size_; }
  /// Aggregated size: own_size + sum of children totals (when known).
  [[nodiscard]] std::uint64_t total_size() const noexcept { return total_size_; }

  [[nodiscard]] std::filesystem::file_time_type mtime() const noexcept { return mtime_; }
  [[nodiscard]] std::filesystem::perms permissions() const noexcept { return permissions_; }
  /// st_dev when available (optional mount-boundary policy).
  [[nodiscard]] bool has_device_id() const noexcept { return has_device_id_; }
  [[nodiscard]] std::uint64_t device_id() const noexcept { return device_id_; }

  [[nodiscard]] const std::vector<std::shared_ptr<const FsTreeNode>>& children() const noexcept
  {
    return children_;
  }

  [[nodiscard]] bool is_directory() const noexcept { return kind_ == FsTreeNodeKind::Directory; }
  [[nodiscard]] bool is_file() const noexcept { return kind_ == FsTreeNodeKind::File; }

  // --- mutation API used by the scanner / cache (not for casual consumers) ---

  void set_path(std::filesystem::path p) { path_ = std::move(p); }
  void set_name(std::string n) { name_ = std::move(n); }
  void set_kind(FsTreeNodeKind k) { kind_ = k; }
  void set_state(FsTreeNodeState s) { state_ = s; }
  void set_own_size(std::uint64_t s) { own_size_ = s; }
  void set_total_size(std::uint64_t s) { total_size_ = s; }
  void set_mtime(std::filesystem::file_time_type t) { mtime_ = t; }
  void set_permissions(std::filesystem::perms p) { permissions_ = p; }
  void set_device_id(std::uint64_t id)
  {
    device_id_ = id;
    has_device_id_ = true;
  }
  void add_child(std::shared_ptr<const FsTreeNode> child) { children_.push_back(std::move(child)); }
  void clear_children() { children_.clear(); }

private:
  std::filesystem::path path_;
  std::string name_;
  FsTreeNodeKind kind_ = FsTreeNodeKind::Other;
  FsTreeNodeState state_ = FsTreeNodeState::Pending;
  std::uint64_t own_size_ = 0;
  std::uint64_t total_size_ = 0;
  std::filesystem::file_time_type mtime_{};
  std::filesystem::perms permissions_{};
  std::uint64_t device_id_ = 0;
  bool has_device_id_ = false;
  std::vector<std::shared_ptr<const FsTreeNode>> children_;
};

} // namespace dirtoo::tree

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/tree/scan_tree.hpp"

#include <chrono>
#include <queue>
#include <system_error>
#include <utility>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace dirtoo::tree {
namespace {

struct BuildNode {
  std::shared_ptr<FsTreeNode> node;
  /// Parent in the build graph (null for root).
  std::shared_ptr<FsTreeNode> parent;
  std::uint32_t depth = 0;
  /// Outstanding directory children still scanning (for bottom-up aggregate).
  int pending_dir_children = 0;
};

[[nodiscard]] bool cancelled(const std::atomic_bool* cancel)
{
  return cancel != nullptr && cancel->load(std::memory_order_relaxed);
}

[[nodiscard]] FsTreeNodeKind kind_from_symlink_status(const std::filesystem::file_status& st)
{
  using std::filesystem::file_type;
  switch (st.type()) {
  case file_type::directory:
    return FsTreeNodeKind::Directory;
  case file_type::regular:
    return FsTreeNodeKind::File;
  case file_type::symlink:
    return FsTreeNodeKind::Symlink;
  default:
    return FsTreeNodeKind::Other;
  }
}

/// Fill metadata from lstat (preferred) or directory_entry status.
void fill_from_path(FsTreeNode& node, const std::filesystem::path& path)
{
  node.set_path(path);
  try {
    node.set_name(path.filename().string());
  } catch (...) {
    node.set_name({});
  }

#if !defined(_WIN32)
  struct stat pst {};
  if (::lstat(path.c_str(), &pst) == 0) {
    if (S_ISLNK(pst.st_mode)) {
      node.set_kind(FsTreeNodeKind::Symlink);
    } else if (S_ISDIR(pst.st_mode)) {
      node.set_kind(FsTreeNodeKind::Directory);
    } else if (S_ISREG(pst.st_mode)) {
      node.set_kind(FsTreeNodeKind::File);
    } else {
      node.set_kind(FsTreeNodeKind::Other);
    }
    if (S_ISREG(pst.st_mode)) {
      node.set_own_size(static_cast<std::uint64_t>(pst.st_size));
    } else {
      node.set_own_size(0);
    }
    node.set_total_size(node.own_size());
    node.set_permissions(static_cast<std::filesystem::perms>(pst.st_mode & 07777));
    node.set_device_id(static_cast<std::uint64_t>(pst.st_dev));
    // file_time_type from seconds; good enough for invalidation stamps.
    const auto secs = std::chrono::seconds(pst.st_mtime);
    node.set_mtime(std::filesystem::file_time_type(
        std::chrono::duration_cast<std::filesystem::file_time_type::duration>(secs)));
    return;
  }
#endif

  std::error_code ec;
  const auto fst = std::filesystem::symlink_status(path, ec);
  if (ec) {
    node.set_kind(FsTreeNodeKind::Other);
    node.set_state(FsTreeNodeState::Failed);
    return;
  }
  node.set_kind(kind_from_symlink_status(fst));
  node.set_permissions(fst.permissions());
  if (node.kind() == FsTreeNodeKind::File) {
    const auto sz = std::filesystem::file_size(path, ec);
    if (!ec) {
      node.set_own_size(static_cast<std::uint64_t>(sz));
    }
  }
  node.set_total_size(node.own_size());
  const auto ft = std::filesystem::last_write_time(path, ec);
  if (!ec) {
    node.set_mtime(ft);
  }
}

void recompute_total_from_children(FsTreeNode& node)
{
  std::uint64_t sum = node.own_size();
  for (const auto& ch : node.children()) {
    if (ch) {
      sum += ch->total_size();
    }
  }
  node.set_total_size(sum);
}

} // namespace

std::shared_ptr<const FsTreeNode>
scan_tree(const std::filesystem::path& root, const ScanOptions& options,
          const std::atomic_bool* cancel, ScanProgressFn progress)
{
  auto root_node = std::make_shared<FsTreeNode>();
  fill_from_path(*root_node, root);

  if (root_node->state() == FsTreeNodeState::Failed) {
    return root_node;
  }

  // Non-directory roots: leaf with own size.
  if (root_node->kind() != FsTreeNodeKind::Directory) {
    // Symlink-to-dir: only descend when follow_symlinks.
    if (root_node->kind() == FsTreeNodeKind::Symlink && options.follow_symlinks) {
      std::error_code ec;
      if (std::filesystem::is_directory(root, ec) && !ec) {
        root_node->set_kind(FsTreeNodeKind::Directory);
      } else {
        root_node->set_state(FsTreeNodeState::Complete);
        return root_node;
      }
    } else {
      root_node->set_state(FsTreeNodeState::Complete);
      return root_node;
    }
  }

  const std::optional<std::uint64_t> root_dev =
      root_node->has_device_id() ? std::optional<std::uint64_t>(root_node->device_id()) : std::nullopt;

  // BFS queue of directories to expand.
  std::queue<BuildNode> q;
  q.push(BuildNode{root_node, nullptr, 0, 0});

  std::uint64_t nodes_seen = 1;
  if (progress) {
    progress(nodes_seen, root);
  }

  // Track build nodes by raw pointer for pending_dir_children updates.
  // Parent shared_ptr kept alive via queue / tree ownership.
  std::vector<std::shared_ptr<FsTreeNode>> dir_holders;
  dir_holders.push_back(root_node);

  while (!q.empty()) {
    if (cancelled(cancel)) {
      // Mark remaining queued dirs Partial.
      while (!q.empty()) {
        auto bn = std::move(q.front());
        q.pop();
        if (bn.node->state() == FsTreeNodeState::Pending) {
          bn.node->set_state(FsTreeNodeState::Partial);
        }
        recompute_total_from_children(*bn.node);
      }
      if (root_node->state() != FsTreeNodeState::Complete) {
        root_node->set_state(FsTreeNodeState::Partial);
      }
      recompute_total_from_children(*root_node);
      return root_node;
    }

    BuildNode bn = std::move(q.front());
    q.pop();
    auto& dir = *bn.node;

    if (options.max_depth && bn.depth >= *options.max_depth) {
      dir.set_state(FsTreeNodeState::Complete);
      dir.set_total_size(dir.own_size());
      continue;
    }

    std::error_code ec;
    const auto opts = std::filesystem::directory_options::skip_permission_denied;
    std::filesystem::directory_iterator it(dir.path(), opts, ec);
    if (ec) {
      dir.set_state(FsTreeNodeState::Failed);
      dir.set_total_size(dir.own_size());
      continue;
    }

    std::vector<std::shared_ptr<FsTreeNode>> child_dirs;

    for (; it != std::filesystem::directory_iterator(); it.increment(ec)) {
      if (ec) {
        ec.clear();
        continue;
      }
      if (cancelled(cancel)) {
        break;
      }

      const auto& entry = *it;
      const auto name = entry.path().filename().string();
      if (name == "." || name == "..") {
        continue;
      }
      if (!options.include_hidden && !name.empty() && name[0] == '.') {
        continue;
      }

      auto child = std::make_shared<FsTreeNode>();
      fill_from_path(*child, entry.path());
      ++nodes_seen;
      if (progress) {
        progress(nodes_seen, child->path());
      }

      // Mount boundary
      if (!options.cross_device && root_dev && child->has_device_id()
          && child->device_id() != *root_dev && child->kind() == FsTreeNodeKind::Directory) {
        child->set_state(FsTreeNodeState::Complete);
        child->set_total_size(child->own_size());
        dir.add_child(child);
        continue;
      }

      if (child->kind() == FsTreeNodeKind::Directory
          || (child->kind() == FsTreeNodeKind::Symlink && options.follow_symlinks
              && [&] {
                   std::error_code e2;
                   return std::filesystem::is_directory(child->path(), e2) && !e2;
                 }())) {
        if (child->kind() == FsTreeNodeKind::Symlink) {
          child->set_kind(FsTreeNodeKind::Directory);
        }
        child->set_state(FsTreeNodeState::Pending);
        dir.add_child(child);
        child_dirs.push_back(child);
      } else {
        child->set_state(FsTreeNodeState::Complete);
        child->set_total_size(child->own_size());
        dir.add_child(child);
      }
    }

    if (cancelled(cancel)) {
      dir.set_state(FsTreeNodeState::Partial);
      for (auto& cd : child_dirs) {
        cd->set_state(FsTreeNodeState::Partial);
      }
      recompute_total_from_children(dir);
      // Fall through to still enqueue nothing; outer loop will drain Partial.
      continue;
    }

    if (child_dirs.empty()) {
      dir.set_state(FsTreeNodeState::Complete);
      recompute_total_from_children(dir);
    } else {
      dir.set_state(FsTreeNodeState::Partial);
      for (auto& cd : child_dirs) {
        q.push(BuildNode{cd, bn.node, bn.depth + 1, 0});
        dir_holders.push_back(cd);
      }
    }
  }

  // Bottom-up: process directories in reverse BFS order (dir_holders is BFS order).
  for (auto it = dir_holders.rbegin(); it != dir_holders.rend(); ++it) {
    auto& n = **it;
    recompute_total_from_children(n);
    if (n.state() == FsTreeNodeState::Partial || n.state() == FsTreeNodeState::Pending) {
      // All children should be terminal now if scan finished without cancel.
      bool any_partial = false;
      bool any_failed = false;
      for (const auto& ch : n.children()) {
        if (!ch) {
          continue;
        }
        if (ch->state() == FsTreeNodeState::Partial || ch->state() == FsTreeNodeState::Pending) {
          any_partial = true;
        }
        if (ch->state() == FsTreeNodeState::Failed) {
          any_failed = true;
        }
      }
      if (any_partial) {
        n.set_state(FsTreeNodeState::Partial);
      } else if (any_failed && n.children().empty()) {
        n.set_state(FsTreeNodeState::Failed);
      } else {
        n.set_state(FsTreeNodeState::Complete);
      }
    }
  }

  return root_node;
}

} // namespace dirtoo::tree

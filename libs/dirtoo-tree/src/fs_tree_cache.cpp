// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/tree/fs_tree_cache.hpp"

#include <system_error>

namespace dirtoo::tree {
namespace {

[[nodiscard]] bool path_is_under_or_equal(const std::string& key, const std::string& prefix)
{
  if (key == prefix) {
    return true;
  }
  if (key.size() <= prefix.size()) {
    return false;
  }
  if (!key.starts_with(prefix)) {
    return false;
  }
  return prefix.back() == '/' || key[prefix.size()] == '/';
}

} // namespace

std::string FsTreeCache::path_key(const std::filesystem::path& path)
{
  std::error_code ec;
  const auto canon = std::filesystem::weakly_canonical(path, ec);
  if (!ec) {
    return canon.string();
  }
  return path.lexically_normal().string();
}

std::shared_ptr<const FsTreeNode>
FsTreeCache::snapshot(const std::filesystem::path& root) const
{
  const auto key = path_key(root);
  std::lock_guard lock(mutex_);
  const auto it = roots_.find(key);
  if (it == roots_.end()) {
    return nullptr;
  }
  return it->second;
}

std::optional<std::uint64_t>
FsTreeCache::entry_total_size(const std::filesystem::path& path) const
{
  const auto key = path_key(path);
  std::lock_guard lock(mutex_);
  const auto it = entry_sizes_.find(key);
  if (it == entry_sizes_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void FsTreeCache::set_entry_total_size(const std::filesystem::path& path, std::uint64_t total_size)
{
  const auto key = path_key(path);
  std::lock_guard lock(mutex_);
  entry_sizes_[key] = total_size;
}

std::shared_ptr<const FsTreeNode>
FsTreeCache::scan(const std::filesystem::path& root, const ScanOptions& options,
                  const std::atomic_bool* cancel, ScanCallbacks callbacks)
{
  // Chain node_ready so progressive sizes land even if the caller omits it.
  ScanCallbacks cb = std::move(callbacks);
  auto user_ready = std::move(cb.node_ready);
  cb.node_ready = [this, user_ready = std::move(user_ready)](const std::filesystem::path& path,
                                                             std::uint64_t total_size,
                                                             FsTreeNodeKind kind) {
    set_entry_total_size(path, total_size);
    if (user_ready) {
      user_ready(path, total_size, kind);
    }
  };

  auto tree = scan_tree(root, options, cancel, std::move(cb));
  const auto key = path_key(root);
  {
    std::lock_guard lock(mutex_);
    roots_[key] = tree;
  }
  return tree;
}

void FsTreeCache::invalidate(const std::filesystem::path& root)
{
  const auto key = path_key(root);
  std::lock_guard lock(mutex_);
  roots_.erase(key);
  for (auto it = entry_sizes_.begin(); it != entry_sizes_.end();) {
    if (path_is_under_or_equal(it->first, key)) {
      it = entry_sizes_.erase(it);
    } else {
      ++it;
    }
  }
}

void FsTreeCache::invalidate_subtree(const std::filesystem::path& root)
{
  const auto prefix = path_key(root);
  std::lock_guard lock(mutex_);
  for (auto it = roots_.begin(); it != roots_.end();) {
    if (path_is_under_or_equal(it->first, prefix)) {
      it = roots_.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = entry_sizes_.begin(); it != entry_sizes_.end();) {
    if (path_is_under_or_equal(it->first, prefix)) {
      it = entry_sizes_.erase(it);
    } else {
      ++it;
    }
  }
}

void FsTreeCache::clear()
{
  std::lock_guard lock(mutex_);
  roots_.clear();
  entry_sizes_.clear();
}

std::size_t FsTreeCache::size() const
{
  std::lock_guard lock(mutex_);
  return roots_.size();
}

} // namespace dirtoo::tree

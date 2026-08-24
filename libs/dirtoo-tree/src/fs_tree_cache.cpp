// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/tree/fs_tree_cache.hpp"

#include <system_error>

namespace dirtoo::tree {

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

std::shared_ptr<const FsTreeNode>
FsTreeCache::scan(const std::filesystem::path& root, const ScanOptions& options,
                  const std::atomic_bool* cancel, ScanProgressFn progress)
{
  auto tree = scan_tree(root, options, cancel, std::move(progress));
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
}

void FsTreeCache::invalidate_subtree(const std::filesystem::path& root)
{
  const auto prefix = path_key(root);
  std::lock_guard lock(mutex_);
  for (auto it = roots_.begin(); it != roots_.end();) {
    const auto& k = it->first;
    if (k == prefix || (k.size() > prefix.size() && k.starts_with(prefix)
                        && (prefix.back() == '/' || k[prefix.size()] == '/'))) {
      it = roots_.erase(it);
    } else {
      ++it;
    }
  }
}

void FsTreeCache::clear()
{
  std::lock_guard lock(mutex_);
  roots_.clear();
}

std::size_t FsTreeCache::size() const
{
  std::lock_guard lock(mutex_);
  return roots_.size();
}

} // namespace dirtoo::tree

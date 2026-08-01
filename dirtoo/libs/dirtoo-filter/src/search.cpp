// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/search.hpp"

#include <system_error>

namespace dirtoo::filter {
namespace {

bool is_hidden_name(const std::filesystem::path& p)
{
  const auto name = p.filename().string();
  return !name.empty() && name[0] == '.';
}

int depth_of(const std::filesystem::path& root, const std::filesystem::path& path)
{
  // Count components relative to root.
  std::error_code ec;
  const auto rel = std::filesystem::relative(path, root, ec);
  if (ec) {
    return 0;
  }
  int depth = 0;
  for (const auto& part : rel) {
    if (part != "." && part != "/") {
      ++depth;
    }
  }
  return depth;
}

FilterItem make_item(const std::filesystem::directory_entry& entry)
{
  FilterItem item;
  item.path = entry.path();
  item.name = entry.path().filename().string();
  std::error_code ec;
  item.is_directory = entry.is_directory(ec);
  if (!item.is_directory) {
    item.size = entry.file_size(ec);
    if (ec) {
      item.size = 0;
    }
  }
  return item;
}

} // namespace

SearchStats search_directory(const std::filesystem::path& root, const MatchFunc& match,
                             const SearchOptions& options,
                             const std::function<void(const FilterItem&)>& on_match)
{
  SearchStats stats;
  std::error_code ec;
  if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
    ++stats.errors;
    return stats;
  }

  // Depth 0: only match children of root (like non-recursive list), or also root?
  // We search *under* root (contents), not the root directory itself.

  auto opts = std::filesystem::directory_options::skip_permission_denied;
  if (options.follow_directory_symlinks) {
    opts |= std::filesystem::directory_options::follow_directory_symlink;
  }

  if (options.max_depth == 0) {
    for (const auto& entry : std::filesystem::directory_iterator(root, opts, ec)) {
      if (options.should_cancel && options.should_cancel()) {
        break;
      }
      if (ec) {
        ++stats.errors;
        ec.clear();
        continue;
      }
      ++stats.visited;
      if (!options.show_hidden && is_hidden_name(entry.path())) {
        continue;
      }
      const auto item = make_item(entry);
      if (match.matches(item)) {
        ++stats.matched;
        if (on_match) {
          on_match(item);
        }
      }
    }
    return stats;
  }

  // recursive
  std::filesystem::recursive_directory_iterator it(
      root, opts, ec);
  const std::filesystem::recursive_directory_iterator end;
  if (ec) {
    ++stats.errors;
    return stats;
  }

  for (; it != end; it.increment(ec)) {
    if (options.should_cancel && options.should_cancel()) {
      break;
    }
    if (ec) {
      ++stats.errors;
      ec.clear();
      continue;
    }

    const auto& entry = *it;
    if (options.max_depth >= 0) {
      // depth of this entry relative to root
      const int depth = depth_of(root, entry.path());
      if (depth > options.max_depth) {
        it.disable_recursion_pending();
        continue;
      }
      // Don't descend further than max_depth
      if (entry.is_directory(ec) && depth >= options.max_depth) {
        it.disable_recursion_pending();
      }
    }

    ++stats.visited;
    if (!options.show_hidden && is_hidden_name(entry.path())) {
      if (entry.is_directory(ec)) {
        it.disable_recursion_pending();
      }
      continue;
    }

    const auto item = make_item(entry);
    if (match.matches(item)) {
      ++stats.matched;
      if (on_match) {
        on_match(item);
      }
    }
  }
  return stats;
}

std::vector<FilterItem>
search_directory_collect(const std::filesystem::path& root, const MatchFunc& match,
                         const SearchOptions& options, std::optional<std::size_t> limit)
{
  std::vector<FilterItem> out;
  SearchOptions opts = options;
  auto original_cancel = options.should_cancel;
  opts.should_cancel = [&] {
    if (limit && out.size() >= *limit) {
      return true;
    }
    return original_cancel && original_cancel();
  };
  search_directory(root, match, opts, [&](const FilterItem& item) {
    if (!limit || out.size() < *limit) {
      out.push_back(item);
    }
  });
  return out;
}

} // namespace dirtoo::filter

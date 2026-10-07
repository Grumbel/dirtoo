// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "archive_listing.hpp"

#include <string_view>
#include <unordered_set>

namespace dirtoo::app {
namespace {

bool read_stamp(const std::filesystem::path& path, std::uintmax_t* size,
                std::filesystem::file_time_type* mtime)
{
  std::error_code ec;
  const auto sz = std::filesystem::file_size(path, ec);
  if (ec) {
    return false;
  }
  const auto mt = std::filesystem::last_write_time(path, ec);
  if (ec) {
    return false;
  }
  *size = sz;
  *mtime = mt;
  return true;
}

} // namespace

void ArchiveListing::clear()
{
  entries_.clear();
  indexed_path_.clear();
  indexed_size_ = 0;
  indexed_mtime_ = {};
  ok_ = false;
}

bool ArchiveListing::stamp_matches(const std::filesystem::path& archive_file,
                                   std::uintmax_t size,
                                   std::filesystem::file_time_type mtime)
{
  std::uintmax_t cur_size = 0;
  std::filesystem::file_time_type cur_mtime{};
  if (!read_stamp(archive_file, &cur_size, &cur_mtime)) {
    return false;
  }
  return cur_size == size && cur_mtime == mtime;
}

bool ArchiveListing::stamp_matches(const std::filesystem::path& archive_file, const Stamp& stamp)
{
  return stamp_matches(archive_file, stamp.size, stamp.mtime);
}

bool ArchiveListing::load(const std::filesystem::path& archive_file, std::string* error_out)
{
  clear();
  auto listed = archive::list_archive_entries(archive_file);
  if (!listed) {
    if (error_out != nullptr) {
      *error_out = listed.error();
    }
    return false;
  }
  entries_ = std::move(*listed);
  indexed_path_ = archive_file;
  std::uintmax_t sz = 0;
  std::filesystem::file_time_type mt{};
  if (read_stamp(archive_file, &sz, &mt)) {
    indexed_size_ = sz;
    indexed_mtime_ = mt;
  }
  ok_ = true;
  return true;
}

bool ArchiveListing::refresh_if_stale(const std::filesystem::path& archive_file,
                                      std::string* error_out)
{
  if (ok_ && indexed_path_ == archive_file
      && stamp_matches(archive_file, indexed_size_, indexed_mtime_)) {
    return true;
  }
  return load(archive_file, error_out);
}

bool ArchiveListing::ready_for(const std::filesystem::path& archive_file) const
{
  return ok_ && indexed_path_ == archive_file;
}

std::vector<fs::FileInfo> ArchiveListing::fileinfos_for(const fs::Location& location) const
{
  if (!ok_) {
    return {};
  }
  return archive::fileinfos_for_prefix(location, entries_);
}

std::unordered_map<std::string, std::int64_t>
ArchiveListing::child_counts_for(const fs::Location& location) const
{
  std::unordered_map<std::string, std::int64_t> counts;
  if (!ok_) {
    return counts;
  }
  const std::string prefix = location.entry_path().lexically_normal().generic_string();
  const std::string base = prefix.empty() ? std::string{} : prefix + "/";

  // One pass over the TOC: for every entry below `base`, the first component is
  // the directory row it belongs to and the second the distinct child to count.
  // (The old per-directory rescan was O(directories × entries).)
  std::unordered_map<std::string, std::unordered_set<std::string>> children;
  for (const auto& entry : entries_) {
    const std::string rel_all = entry.path.generic_string();
    if (!rel_all.starts_with(base)) {
      continue;
    }
    const std::string_view rel = std::string_view{rel_all}.substr(base.size());
    const auto slash = rel.find('/');
    if (slash == std::string_view::npos) {
      continue;  // a direct child itself, not an entry inside a directory row
    }
    const std::string dir{rel.substr(0, slash)};
    const std::string_view inner = rel.substr(slash + 1);
    if (inner.empty()) {
      continue;
    }
    const auto next = inner.find('/');
    children[dir].emplace(inner.substr(0, next));
  }

  for (const auto& fi : fileinfos_for(location)) {
    if (!fi.is_directory()) {
      continue;
    }
    const auto it = children.find(fi.basename());
    counts.emplace(fi.path().string(), it == children.end()
                                           ? 0
                                           : static_cast<std::int64_t>(it->second.size()));
  }
  return counts;
}

} // namespace dirtoo::app

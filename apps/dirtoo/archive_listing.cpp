// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "archive_listing.hpp"

#include <set>

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
  const auto items = fileinfos_for(location);
  for (const auto& fi : items) {
    if (!fi.is_directory()) {
      continue;
    }
    const std::string name = fi.basename();
    const std::string needed =
        prefix.empty() ? name + "/" : prefix + "/" + name + "/";
    std::int64_t n = 0;
    std::set<std::string> seen;
    for (const auto& entry : entries_) {
      std::string rel = entry.path.generic_string();
      if (!rel.starts_with(needed)) {
        continue;
      }
      rel = rel.substr(needed.size());
      if (rel.empty()) {
        continue;
      }
      const auto slash = rel.find('/');
      const std::string child = (slash == std::string::npos) ? rel : rel.substr(0, slash);
      if (seen.insert(child).second) {
        ++n;
      }
    }
    counts.emplace(fi.path().string(), n);
  }
  return counts;
}

} // namespace dirtoo::app

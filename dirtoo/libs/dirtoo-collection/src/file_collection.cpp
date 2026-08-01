// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/collection/file_collection.hpp"

#include <algorithm>
#include <cctype>

namespace dirtoo::collection {
namespace {

std::string to_lower(std::string s)
{
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

bool is_hidden_name(const std::string& name)
{
  return !name.empty() && name[0] == '.';
}

} // namespace

void FileCollection::clear()
{
  items_.clear();
  visible_.clear();
  name_filter_.clear();
}

void FileCollection::set_items(std::vector<fs::FileInfo> items)
{
  items_ = std::move(items);
  rebuild_visible();
}

void FileCollection::add(fs::FileInfo info)
{
  items_.push_back(std::move(info));
  rebuild_visible();
}

bool FileCollection::remove(const fs::Location& location)
{
  const auto it = std::ranges::find_if(items_, [&](const fs::FileInfo& fi) {
    return fi.location() == location;
  });
  if (it == items_.end()) {
    return false;
  }
  items_.erase(it);
  rebuild_visible();
  return true;
}

std::optional<std::size_t> FileCollection::index_of(const fs::Location& location) const
{
  for (std::size_t i = 0; i < visible_.size(); ++i) {
    if (visible_[i].location() == location) {
      return i;
    }
  }
  return std::nullopt;
}

void FileCollection::sort_by_name(bool ascending)
{
  std::ranges::sort(items_, [ascending](const fs::FileInfo& a, const fs::FileInfo& b) {
    const int cmp = a.basename().compare(b.basename());
    return ascending ? cmp < 0 : cmp > 0;
  });
  rebuild_visible();
}

void FileCollection::sort_by_size(bool ascending)
{
  std::ranges::sort(items_, [ascending](const fs::FileInfo& a, const fs::FileInfo& b) {
    return ascending ? a.size() < b.size() : a.size() > b.size();
  });
  rebuild_visible();
}

void FileCollection::sort_by_mtime(bool ascending)
{
  std::ranges::sort(items_, [ascending](const fs::FileInfo& a, const fs::FileInfo& b) {
    return ascending ? a.mtime() < b.mtime() : a.mtime() > b.mtime();
  });
  rebuild_visible();
}

void FileCollection::set_name_filter(std::string needle)
{
  name_filter_ = to_lower(std::move(needle));
  rebuild_visible();
}

void FileCollection::clear_filter()
{
  name_filter_.clear();
  rebuild_visible();
}

void FileCollection::set_show_hidden(bool show)
{
  show_hidden_ = show;
  rebuild_visible();
}

const std::vector<fs::FileInfo>& FileCollection::visible_items() const noexcept
{
  return visible_;
}

void FileCollection::rebuild_visible()
{
  visible_.clear();
  for (const auto& fi : items_) {
    if (!show_hidden_ && is_hidden_name(fi.basename())) {
      continue;
    }
    if (!name_filter_.empty()) {
      if (to_lower(fi.basename()).find(name_filter_) == std::string::npos) {
        continue;
      }
    }
    visible_.push_back(fi);
  }
}

} // namespace dirtoo::collection

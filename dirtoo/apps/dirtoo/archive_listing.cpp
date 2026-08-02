// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "archive_listing.hpp"

namespace dirtoo::app {

void ArchiveListing::clear()
{
  entries_.clear();
  indexed_path_.clear();
  ok_ = false;
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
  ok_ = true;
  return true;
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

} // namespace dirtoo::app

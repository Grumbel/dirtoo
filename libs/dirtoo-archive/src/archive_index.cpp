// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/archive/archive_index.hpp"

#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <set>

namespace dirtoo::archive {

std::expected<std::vector<ArchiveEntry>, std::string>
list_archive_entries(const std::filesystem::path& archive_file)
{
  // Flake guarantees libarchive; no CLI fallback.
  return list_archive_entries_libarchive(archive_file);
}

std::vector<fs::FileInfo>
fileinfos_for_prefix(const fs::Location& archive_location,
                     const std::vector<ArchiveEntry>& entries)
{
  const std::filesystem::path prefix = archive_location.entry_path().lexically_normal();
  const std::string prefix_str = prefix.generic_string();

  std::set<std::string> seen;
  std::vector<fs::FileInfo> result;

  for (const auto& entry : entries) {
    std::string rel = entry.path.generic_string();
    if (!prefix_str.empty()) {
      if (rel == prefix_str) {
        continue;
      }
      const std::string needed = prefix_str + "/";
      if (!rel.starts_with(needed)) {
        continue;
      }
      rel = rel.substr(needed.size());
    }
    if (rel.empty()) {
      continue;
    }

    const auto slash = rel.find('/');
    std::string name;
    bool is_dir = false;
    std::uint64_t size = 0;
    if (slash == std::string::npos) {
      name = rel;
      is_dir = entry.is_directory;
      size = entry.size;
    } else {
      name = rel.substr(0, slash);
      is_dir = true;
    }

    if (!seen.insert(name).second) {
      continue;
    }

    result.push_back(
        fs::FileInfo::synthetic(archive_location.join(name), name, is_dir, size));
  }

  return result;
}

std::expected<std::filesystem::path, std::string>
extract_member(const std::filesystem::path& archive_file,
               const std::filesystem::path& member,
               const std::filesystem::path& dest_dir)
{
  // Flake guarantees libarchive; no CLI fallback.
  return extract_member_libarchive(archive_file, member, dest_dir);
}

} // namespace dirtoo::archive

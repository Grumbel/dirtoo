// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/fs/location.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace dirtoo::archive {

struct ArchiveEntry {
  std::filesystem::path path; // relative path inside archive (generic)
  bool is_directory = false;
  std::uint64_t size = 0;
};

/// List archive members using external tools (bsdtar -tf, unzip -Z1, 7z l).
/// Does not extract data.
[[nodiscard]] std::expected<std::vector<ArchiveEntry>, std::string>
list_archive_entries(const std::filesystem::path& archive_file);

/// Build FileInfo-like listing for one directory level under `prefix` (no trailing slash).
/// Names are immediate children only.
[[nodiscard]] std::vector<fs::FileInfo>
fileinfos_for_prefix(const fs::Location& archive_location,
                     const std::vector<ArchiveEntry>& entries);

/// Extract a single member to dest_dir (parent dirs created). Returns path to file.
[[nodiscard]] std::expected<std::filesystem::path, std::string>
extract_member(const std::filesystem::path& archive_file,
               const std::filesystem::path& member,
               const std::filesystem::path& dest_dir);

} // namespace dirtoo::archive

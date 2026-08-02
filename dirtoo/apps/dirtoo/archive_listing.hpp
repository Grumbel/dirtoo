// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/archive/archive_index.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/fs/location.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace dirtoo::app {

/// In-memory TOC for the archive currently open in the main view.
/// Keeps MainWindow from owning listing bookkeeping inline (S3 factoring).
class ArchiveListing {
public:
  void clear();

  /// Load TOC for @p archive_file via libarchive. Returns false on failure.
  [[nodiscard]] bool load(const std::filesystem::path& archive_file, std::string* error_out);

  /// Re-list only if the archive file's size or mtime changed since the last load.
  /// Returns true when the in-memory index is usable afterward.
  [[nodiscard]] bool refresh_if_stale(const std::filesystem::path& archive_file,
                                      std::string* error_out = nullptr);

  /// True when index matches @p archive_file and is ready.
  [[nodiscard]] bool ready_for(const std::filesystem::path& archive_file) const;

  [[nodiscard]] bool ok() const noexcept { return ok_; }
  [[nodiscard]] const std::filesystem::path& indexed_path() const noexcept { return indexed_path_; }
  [[nodiscard]] const std::vector<archive::ArchiveEntry>& entries() const noexcept { return entries_; }

  /// Immediate children of @p location inside the indexed archive.
  [[nodiscard]] std::vector<fs::FileInfo> fileinfos_for(const fs::Location& location) const;

  /// Non-recursive child counts for directory rows at @p location (path string → count).
  [[nodiscard]] std::unordered_map<std::string, std::int64_t>
  child_counts_for(const fs::Location& location) const;

private:
  [[nodiscard]] static bool stamp_matches(const std::filesystem::path& archive_file,
                                          std::uintmax_t size,
                                          std::filesystem::file_time_type mtime);

  std::vector<archive::ArchiveEntry> entries_;
  std::filesystem::path indexed_path_;
  std::uintmax_t indexed_size_ = 0;
  std::filesystem::file_time_type indexed_mtime_{};
  bool ok_ = false;
};

} // namespace dirtoo::app

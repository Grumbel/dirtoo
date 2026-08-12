// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/tags/tag_def.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dirtoo::hash {
class ChecksumStore;
}

namespace dirtoo::tags {

struct TaggedFile {
  std::int64_t file_id = 0;
  std::string sha256;
  std::vector<std::string> paths;
  std::vector<std::string> tags;
};

/// Tag association DB. Never hashes files; identity is SHA-256 from ChecksumStore.
/// Default path: $XDG_DATA_HOME/dirtoo/tags.sqlite
///
/// Indirection: file_tags stores integer tag_id → tag_defs.id. Renaming a tag only
/// updates tag_defs.name (and optionally label); file associations stay put.
class TagStore {
public:
  TagStore() = default;
  ~TagStore();

  TagStore(const TagStore&) = delete;
  TagStore& operator=(const TagStore&) = delete;
  TagStore(TagStore&&) noexcept;
  TagStore& operator=(TagStore&&) noexcept;

  [[nodiscard]] static std::filesystem::path default_path();

  [[nodiscard]] bool open(std::filesystem::path db_path, std::string* error = nullptr);
  void close();
  [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

  // --- tag definitions ---
  [[nodiscard]] std::optional<TagDef> ensure_tag(std::string_view name, std::string* error = nullptr);
  [[nodiscard]] std::optional<TagDef> get_tag(std::string_view name) const;
  [[nodiscard]] std::vector<TagDef> list_tags() const;
  bool set_tag_meta(std::string_view name, std::optional<std::string> label,
                    std::optional<std::string> color, std::optional<std::string> badge,
                    std::string* error = nullptr);
  /// Rename tag definition by stable id. Does not touch file_tags rows.
  bool rename_tag(std::string_view old_name, std::string_view new_name,
                  std::string* error = nullptr);

  // --- identity (sha256) ---
  /// Upsert file row for sha256; optionally remember path alias.
  [[nodiscard]] std::optional<std::int64_t>
  ensure_file_sha256(std::string_view sha256, std::string_view path_key = {},
                     std::string* error = nullptr);

  /// Resolve path via ChecksumStore only (no hashing). nullopt if checksum unknown.
  [[nodiscard]] std::optional<std::int64_t>
  resolve_path(const dirtoo::hash::ChecksumStore& checksums, std::string_view path_key,
               std::string* error = nullptr);

  // --- tagging ---
  bool add_tag_to_file(std::int64_t file_id, std::string_view tag_name, std::string* error = nullptr);
  bool remove_tag_from_file(std::int64_t file_id, std::string_view tag_name,
                            std::string* error = nullptr);

  [[nodiscard]] std::vector<std::string> tags_for_file(std::int64_t file_id) const;
  [[nodiscard]] std::vector<std::string> tags_for_sha256(std::string_view sha256) const;
  [[nodiscard]] std::vector<std::string> tags_for_path(const dirtoo::hash::ChecksumStore& checksums,
                                                      std::string_view path_key) const;

  [[nodiscard]] std::vector<TaggedFile> files_for_tag(std::string_view tag_name) const;

private:
  bool ensure_schema(std::string* error);
  void* db_ = nullptr;
  std::filesystem::path path_;
};

} // namespace dirtoo::tags

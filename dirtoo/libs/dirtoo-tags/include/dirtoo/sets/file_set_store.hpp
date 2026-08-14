// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dirtoo::sets {

/// One ad-hoc file set (anonymous by default; optional label/color).
struct FileSet {
  std::string id;   ///< Stable UUID string (no braces).
  std::string label; ///< Optional user label (may be empty).
  std::string color; ///< Optional #RRGGBB / #AARRGGBB.
  std::int64_t created_at = 0; ///< Unix seconds.
  std::int64_t updated_at = 0;
  std::int64_t member_count = 0; ///< Filled by list_sets / get_set when available.
};

/// Membership row (path-primary; sha256 optional when known).
struct FileSetMember {
  std::string path_key;
  std::string sha256; ///< Empty if unknown.
};

/// Persistent ad-hoc file sets (“these files belong together”).
/// Path-keyed membership does not require hashing; optional sha256 enables
/// later content-based matching. Sets may overlap. Separate from tags and from
/// View → Group By.
class FileSetStore {
public:
  FileSetStore() = default;
  explicit FileSetStore(std::filesystem::path db_path);
  ~FileSetStore();

  FileSetStore(const FileSetStore&) = delete;
  FileSetStore& operator=(const FileSetStore&) = delete;
  FileSetStore(FileSetStore&&) noexcept;
  FileSetStore& operator=(FileSetStore&&) noexcept;

  [[nodiscard]] static std::filesystem::path default_path();

  [[nodiscard]] bool open(std::filesystem::path db_path, std::string* error = nullptr);
  void close();
  [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

  /// Create a set; optional label/color. Returns the new set (with id).
  [[nodiscard]] std::optional<FileSet>
  create_set(std::string_view label = {}, std::string_view color = {},
             std::string* error = nullptr);

  [[nodiscard]] std::optional<FileSet> get_set(std::string_view set_id) const;
  [[nodiscard]] std::vector<FileSet> list_sets() const;

  bool set_label(std::string_view set_id, std::string_view label, std::string* error = nullptr);
  bool set_color(std::string_view set_id, std::string_view color, std::string* error = nullptr);

  /// Delete set and all membership rows. Files on disk are untouched.
  bool delete_set(std::string_view set_id, std::string* error = nullptr);

  bool add_member(std::string_view set_id, std::string_view path_key,
                  std::string_view sha256 = {}, std::string* error = nullptr);
  bool remove_member(std::string_view set_id, std::string_view path_key,
                     std::string* error = nullptr);

  /// Add many paths; returns number newly inserted (already present skipped).
  [[nodiscard]] int add_members(std::string_view set_id,
                                const std::vector<std::string>& path_keys,
                                std::string* error = nullptr);

  [[nodiscard]] std::vector<FileSetMember> members(std::string_view set_id) const;
  [[nodiscard]] std::int64_t member_count(std::string_view set_id) const;

  /// Sets that contain this path_key (overlap allowed).
  [[nodiscard]] std::vector<FileSet> sets_for_path(std::string_view path_key) const;

  /// True if path is in the set.
  [[nodiscard]] bool contains(std::string_view set_id, std::string_view path_key) const;

private:
  bool ensure_schema(std::string* error);
  [[nodiscard]] static std::string new_id();
  void* db_ = nullptr; // sqlite3*
  std::filesystem::path path_;
};

} // namespace dirtoo::sets

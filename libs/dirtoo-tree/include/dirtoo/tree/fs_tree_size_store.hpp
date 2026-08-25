// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dirtoo::tree {

/// Durable path → total_size index (SQLite).
///
/// Does not store the full tree shape — only sizes seen by scans. Enough for
/// Contents column / progressive UI after restart; structure still comes from
/// a live scan. Default path: $XDG_CACHE_HOME/dirtoo/fstree-sizes.sqlite
class FsTreeSizeStore {
public:
  FsTreeSizeStore() = default;
  explicit FsTreeSizeStore(std::filesystem::path db_path);
  ~FsTreeSizeStore();

  FsTreeSizeStore(const FsTreeSizeStore&) = delete;
  FsTreeSizeStore& operator=(const FsTreeSizeStore&) = delete;
  FsTreeSizeStore(FsTreeSizeStore&&) noexcept;
  FsTreeSizeStore& operator=(FsTreeSizeStore&&) noexcept;

  /// Open (or create) the database. Returns false on failure.
  bool open(std::filesystem::path db_path);
  void close();
  [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

  void upsert(std::string_view path_key, std::uint64_t total_size);
  [[nodiscard]] std::optional<std::uint64_t> get(std::string_view path_key) const;

  /// Load every row into @p out (path_key → total_size).
  void load_all(std::unordered_map<std::string, std::uint64_t>& out) const;

  /// Delete exact key or every key under @p path_prefix (directory boundary).
  void erase(std::string_view path_key);
  void erase_under(std::string_view path_prefix);

  void clear();

  /// Suggested default DB location.
  [[nodiscard]] static std::filesystem::path default_db_path();

private:
  void* db_ = nullptr; // sqlite3*
  std::filesystem::path path_;
};

} // namespace dirtoo::tree

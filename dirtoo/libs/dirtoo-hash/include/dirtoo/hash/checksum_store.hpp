// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/hash/digests.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dirtoo::hash {

class HashError;

/// SQLite-backed checksum cache. Does not hash; only stores/looks up digests.
/// Default path: $XDG_CACHE_HOME/dirtoo/checksums.sqlite (or ~/.cache/…).
class ChecksumStore {
public:
  ChecksumStore() = default;
  explicit ChecksumStore(std::filesystem::path db_path);
  ~ChecksumStore();

  ChecksumStore(const ChecksumStore&) = delete;
  ChecksumStore& operator=(const ChecksumStore&) = delete;
  ChecksumStore(ChecksumStore&&) noexcept;
  ChecksumStore& operator=(ChecksumStore&&) noexcept;

  [[nodiscard]] static std::filesystem::path default_path();

  [[nodiscard]] bool open(std::filesystem::path db_path, std::string* error = nullptr);
  void close();
  [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

  /// Return cached digests if path exists in DB and size+mtime still match @p on_disk.
  [[nodiscard]] std::optional<FileDigests>
  get_if_valid(std::string_view path_key, std::uint64_t size,
               std::optional<std::int64_t> mtime_ns) const;

  /// Unconditional lookup (may be stale).
  [[nodiscard]] std::optional<FileDigests> get(std::string_view path_key) const;

  void put(std::string_view path_key, const FileDigests& digests);
  void remove(std::string_view path_key);

  /// Paths known for a given digest (algo: "sha256"|"md5"|"sha1"|"crc32").
  [[nodiscard]] std::vector<std::string>
  paths_for_hash(std::string_view algo, std::string_view hex) const;

  /// Ensure digests for path: cache hit if valid, else hash_file + put.
  /// @p path_key should be a stable absolute path string.
  [[nodiscard]] std::optional<FileDigests>
  ensure(const std::filesystem::path& path, std::string_view path_key,
         bool refresh, HashError* error = nullptr);

private:
  bool ensure_schema(std::string* error);
  void* db_ = nullptr; // sqlite3*
  std::filesystem::path path_;
};

} // namespace dirtoo::hash

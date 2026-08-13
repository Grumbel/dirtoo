// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/hash/digests.hpp"
#include "dirtoo/hash/hash_file.hpp"

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace dirtoo::app {

/// Process-wide checksum store + full/quick hash helpers.
/// Serializes SQLite access so TagJob, ChecksumDialog, and future idle scanners
/// share one writer path (D4 / D5 foundation). Hash CPU work still runs on the
/// caller's thread; only store I/O is locked.
class HashService {
public:
  static HashService& instance();

  HashService(const HashService&) = delete;
  HashService& operator=(const HashService&) = delete;

  /// Open default checksum DB if not already open.
  [[nodiscard]] bool ensure_open(std::string* error = nullptr);

  [[nodiscard]] bool is_open() const;

  /// Full-file ensure (cache hit or hash_file + put). Thread-safe store access.
  [[nodiscard]] std::optional<dirtoo::hash::FileDigests>
  ensure_full(const std::filesystem::path& path, std::string_view path_key, bool refresh,
              dirtoo::hash::HashError* error = nullptr);

  [[nodiscard]] std::optional<dirtoo::hash::FileDigests> get_full(std::string_view path_key);

  void put_full(std::string_view path_key, const dirtoo::hash::FileDigests& digests);

  [[nodiscard]] std::optional<dirtoo::hash::FileDigests> get_quick(std::string_view path_key);

  void put_quick(std::string_view path_key, const dirtoo::hash::FileDigests& digests);

  [[nodiscard]] bool has_full(std::string_view path_key);

  /// Remove a path key (full + quick namespaces).
  void remove(std::string_view path_key);

  /// Direct store access under lock — prefer the helpers above.
  template <typename Fn>
  auto with_store(Fn&& fn) -> decltype(fn(std::declval<dirtoo::hash::ChecksumStore&>()))
  {
    std::lock_guard lock(mu_);
    return fn(store_);
  }

private:
  HashService() = default;

  mutable std::mutex mu_;
  dirtoo::hash::ChecksumStore store_;
};

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hash_service.hpp"
#include "dirtoo/hash/file_stamp.hpp"

#include <filesystem>

namespace dirtoo::app {

HashService& HashService::instance()
{
  static HashService svc;
  return svc;
}

bool HashService::ensure_open(std::string* error)
{
  std::lock_guard lock(mu_);
  if (store_.is_open()) {
    return true;
  }
  return store_.open(dirtoo::hash::ChecksumStore::default_path(), error);
}

bool HashService::is_open() const
{
  std::lock_guard lock(mu_);
  return store_.is_open();
}

std::optional<dirtoo::hash::FileDigests>
HashService::ensure_full(const std::filesystem::path& path, std::string_view path_key,
                         bool refresh, dirtoo::hash::HashError* error,
                         const dirtoo::hash::HashOptions& hash_options)
{
  std::error_code ec;
  std::uint64_t size = 0;
  std::optional<std::int64_t> mtime_ns;
  if (const auto stamp = dirtoo::hash::stat_stamp(path)) {
    size = stamp->size;
    mtime_ns = stamp->mtime_ns;  // Unix nanoseconds
  } else {
    ec = std::make_error_code(std::errc::no_such_file_or_directory);
  }

  // Cache lookup under lock only.
  {
    std::lock_guard lock(mu_);
    if (!store_.is_open()) {
      std::string open_err;
      if (!store_.open(dirtoo::hash::ChecksumStore::default_path(), &open_err)) {
        if (error != nullptr) {
          error->message = open_err.empty() ? "checksum store not open" : open_err;
        }
        return std::nullopt;
      }
    }
    if (!refresh && !ec) {
      if (auto hit = store_.get_if_valid(path_key, size, mtime_ns)) {
        return hit;
      }
    }
  }

  // Hash without holding the store mutex (long-running I/O + CPU).
  auto digests = dirtoo::hash::hash_file(path, hash_options, error);
  if (!digests) {
    return std::nullopt;
  }
  if (mtime_ns) {
    digests->mtime_ns = mtime_ns;
  }

  {
    std::lock_guard lock(mu_);
    if (!store_.is_open()) {
      std::string open_err;
      if (!store_.open(dirtoo::hash::ChecksumStore::default_path(), &open_err)) {
        if (error != nullptr) {
          error->message = open_err.empty() ? "checksum store not open" : open_err;
        }
        return std::nullopt;
      }
    }
    store_.put(path_key, *digests);
  }
  return digests;
}

std::optional<dirtoo::hash::FileDigests>
HashService::get_full(std::string_view path_key,
                      const std::optional<dirtoo::hash::FileStamp>& stamp)
{
  std::lock_guard lock(mu_);
  if (!store_.is_open()) {
    return std::nullopt;
  }
  if (stamp) {
    return store_.get_if_valid(path_key, stamp->size, stamp->mtime_ns);
  }
  return store_.get(path_key);
}

void HashService::put_full(std::string_view path_key, const dirtoo::hash::FileDigests& digests)
{
  std::lock_guard lock(mu_);
  if (!store_.is_open()) {
    return;
  }
  store_.put(path_key, digests);
}

std::optional<dirtoo::hash::FileDigests> HashService::get_quick(std::string_view path_key)
{
  std::lock_guard lock(mu_);
  if (!store_.is_open()) {
    return std::nullopt;
  }
  return store_.get_quick(path_key);
}

void HashService::put_quick(std::string_view path_key, const dirtoo::hash::FileDigests& digests)
{
  std::lock_guard lock(mu_);
  if (!store_.is_open()) {
    return;
  }
  store_.put_quick(path_key, digests);
}

bool HashService::has_full(std::string_view path_key)
{
  std::lock_guard lock(mu_);
  if (!store_.is_open()) {
    return false;
  }
  return store_.has_full(path_key);
}

void HashService::remove(std::string_view path_key)
{
  std::lock_guard lock(mu_);
  if (!store_.is_open()) {
    return;
  }
  store_.remove(path_key);
}

} // namespace dirtoo::app

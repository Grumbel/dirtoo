// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hash_service.hpp"

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
                         bool refresh, dirtoo::hash::HashError* error)
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
  return store_.ensure(path, path_key, refresh, error);
}

std::optional<dirtoo::hash::FileDigests> HashService::get_full(std::string_view path_key)
{
  std::lock_guard lock(mu_);
  if (!store_.is_open()) {
    return std::nullopt;
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

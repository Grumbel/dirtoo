// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/hash/hash_file.hpp"

#include <sqlite3.h>

#include <cstdlib>
#include <ctime>

namespace dirtoo::hash {
namespace {

constexpr const char* kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS checksums (
  path TEXT PRIMARY KEY NOT NULL,
  size INTEGER NOT NULL,
  mtime_ns INTEGER,
  crc32 TEXT NOT NULL,
  md5 TEXT NOT NULL,
  sha1 TEXT NOT NULL,
  sha256 TEXT NOT NULL,
  last_hashed INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_checksums_sha256 ON checksums(sha256);
CREATE INDEX IF NOT EXISTS idx_checksums_md5 ON checksums(md5);
CREATE INDEX IF NOT EXISTS idx_checksums_sha1 ON checksums(sha1);
CREATE INDEX IF NOT EXISTS idx_checksums_crc32 ON checksums(crc32);
)SQL";

FileDigests row_to_digests(sqlite3_stmt* stmt)
{
  FileDigests d;
  d.size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 1));
  if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
    d.mtime_ns = sqlite3_column_int64(stmt, 2);
  }
  d.crc32_hex = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
  d.md5_hex = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
  d.sha1_hex = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
  d.sha256_hex = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
  return d;
}

} // namespace

ChecksumStore::ChecksumStore(std::filesystem::path db_path)
{
  open(std::move(db_path));
}

ChecksumStore::~ChecksumStore()
{
  close();
}

ChecksumStore::ChecksumStore(ChecksumStore&& other) noexcept
    : db_(other.db_)
    , path_(std::move(other.path_))
{
  other.db_ = nullptr;
}

ChecksumStore& ChecksumStore::operator=(ChecksumStore&& other) noexcept
{
  if (this != &other) {
    close();
    db_ = other.db_;
    path_ = std::move(other.path_);
    other.db_ = nullptr;
  }
  return *this;
}

std::filesystem::path ChecksumStore::default_path()
{
  const char* cache = std::getenv("XDG_CACHE_HOME");
  std::filesystem::path dir;
  if (cache != nullptr && cache[0] != '\0') {
    dir = std::filesystem::path{cache} / "dirtoo";
  } else {
    const char* home = std::getenv("HOME");
    dir = std::filesystem::path{home != nullptr ? home : "."} / ".cache" / "dirtoo";
  }
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir / "checksums.sqlite";
}

bool ChecksumStore::open(std::filesystem::path db_path, std::string* error)
{
  close();
  path_ = std::move(db_path);
  if (auto parent = path_.parent_path(); !parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
  }
  sqlite3* raw = nullptr;
  if (sqlite3_open(path_.string().c_str(), &raw) != SQLITE_OK) {
    if (error) {
      *error = raw ? sqlite3_errmsg(raw) : "sqlite3_open failed";
    }
    if (raw) {
      sqlite3_close(raw);
    }
    return false;
  }
  db_ = raw;
  if (!ensure_schema(error)) {
    close();
    return false;
  }
  return true;
}

void ChecksumStore::close()
{
  if (db_ != nullptr) {
    sqlite3_close(static_cast<sqlite3*>(db_));
    db_ = nullptr;
  }
}

bool ChecksumStore::ensure_schema(std::string* error)
{
  char* err = nullptr;
  if (sqlite3_exec(static_cast<sqlite3*>(db_), kSchema, nullptr, nullptr, &err) != SQLITE_OK) {
    if (error) {
      *error = err ? err : "schema failed";
    }
    if (err) {
      sqlite3_free(err);
    }
    return false;
  }
  return true;
}

std::optional<FileDigests> ChecksumStore::get(std::string_view path_key) const
{
  if (db_ == nullptr) {
    return std::nullopt;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "SELECT path, size, mtime_ns, crc32, md5, sha1, sha256 FROM checksums WHERE path = ?1";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, path_key.data(), static_cast<int>(path_key.size()), SQLITE_TRANSIENT);
  std::optional<FileDigests> out;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    out = row_to_digests(stmt);
  }
  sqlite3_finalize(stmt);
  return out;
}

std::optional<FileDigests>
ChecksumStore::get_if_valid(std::string_view path_key, std::uint64_t size,
                            std::optional<std::int64_t> mtime_ns) const
{
  auto cached = get(path_key);
  if (!cached) {
    return std::nullopt;
  }
  if (cached->size != size) {
    return std::nullopt;
  }
  if (mtime_ns && cached->mtime_ns && *mtime_ns != *cached->mtime_ns) {
    return std::nullopt;
  }
  if (mtime_ns && !cached->mtime_ns) {
    return std::nullopt;
  }
  return cached;
}

void ChecksumStore::put(std::string_view path_key, const FileDigests& digests)
{
  if (db_ == nullptr) {
    return;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "INSERT INTO checksums(path, size, mtime_ns, crc32, md5, sha1, sha256, last_hashed) "
      "VALUES(?1,?2,?3,?4,?5,?6,?7,?8) "
      "ON CONFLICT(path) DO UPDATE SET "
      "size=excluded.size, mtime_ns=excluded.mtime_ns, crc32=excluded.crc32, "
      "md5=excluded.md5, sha1=excluded.sha1, sha256=excluded.sha256, "
      "last_hashed=excluded.last_hashed";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  const auto now = static_cast<std::int64_t>(std::time(nullptr));
  sqlite3_bind_text(stmt, 1, path_key.data(), static_cast<int>(path_key.size()), SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(digests.size));
  if (digests.mtime_ns) {
    sqlite3_bind_int64(stmt, 3, *digests.mtime_ns);
  } else {
    sqlite3_bind_null(stmt, 3);
  }
  sqlite3_bind_text(stmt, 4, digests.crc32_hex.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, digests.md5_hex.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, digests.sha1_hex.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, digests.sha256_hex.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 8, now);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void ChecksumStore::remove(std::string_view path_key)
{
  if (db_ == nullptr) {
    return;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql = "DELETE FROM checksums WHERE path = ?1";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  sqlite3_bind_text(stmt, 1, path_key.data(), static_cast<int>(path_key.size()), SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

std::vector<std::string>
ChecksumStore::paths_for_hash(std::string_view algo, std::string_view hex) const
{
  std::vector<std::string> out;
  if (db_ == nullptr) {
    return out;
  }
  std::string column;
  if (algo == "sha256") {
    column = "sha256";
  } else if (algo == "md5") {
    column = "md5";
  } else if (algo == "sha1") {
    column = "sha1";
  } else if (algo == "crc32") {
    column = "crc32";
  } else {
    return out;
  }
  const std::string sql = "SELECT path FROM checksums WHERE " + column + " = ?1";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return out;
  }
  sqlite3_bind_text(stmt, 1, hex.data(), static_cast<int>(hex.size()), SQLITE_TRANSIENT);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (p) {
      out.emplace_back(p);
    }
  }
  sqlite3_finalize(stmt);
  return out;
}

std::optional<FileDigests>
ChecksumStore::ensure(const std::filesystem::path& path, std::string_view path_key, bool refresh,
                      HashError* error)
{
  std::error_code ec;
  const auto size = static_cast<std::uint64_t>(std::filesystem::file_size(path, ec));
  if (ec) {
    if (error) {
      error->message = "file_size failed: " + ec.message();
    }
    return std::nullopt;
  }
  std::optional<std::int64_t> mtime_ns;
  {
    const auto ftime = std::filesystem::last_write_time(path, ec);
    if (!ec) {
      mtime_ns = static_cast<std::int64_t>(ftime.time_since_epoch().count());
    }
  }

  if (!refresh) {
    if (auto hit = get_if_valid(path_key, size, mtime_ns)) {
      return hit;
    }
  }

  auto digests = hash_file(path, {}, error);
  if (!digests) {
    return std::nullopt;
  }
  // Prefer on-disk mtime we already measured for consistent validity.
  if (mtime_ns) {
    digests->mtime_ns = mtime_ns;
  }
  put(path_key, *digests);
  return digests;
}

} // namespace dirtoo::hash

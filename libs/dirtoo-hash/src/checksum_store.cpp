// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/hash/hash_file.hpp"
#include "dirtoo/hash/file_stamp.hpp"
#include "dirtoo/hash/sqlite_open.hpp"

#include <sqlite3.h>

#include <cstdio>
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

std::string column_text(sqlite3_stmt* stmt, int col)
{
  const auto* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
  return p != nullptr ? std::string{p} : std::string{};
}

FileDigests row_to_digests(sqlite3_stmt* stmt)
{
  FileDigests d;
  d.size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 1));
  if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
    d.mtime_ns = sqlite3_column_int64(stmt, 2);
  }
  d.crc32_hex = column_text(stmt, 3);
  d.md5_hex = column_text(stmt, 4);
  d.sha1_hex = column_text(stmt, 5);
  d.sha256_hex = column_text(stmt, 6);
  return d;
}

} // namespace

bool ChecksumStore::report_db_error(std::string* error, const char* what) const
{
  const char* msg = db_ != nullptr ? sqlite3_errmsg(static_cast<sqlite3*>(db_)) : "no database";
  std::fprintf(stderr, "dirtoo: checksum store %s failed: %s\n", what, msg);
  if (error != nullptr) {
    *error = msg;
  }
  return false;
}

ChecksumStore::ChecksumStore(std::filesystem::path db_path)
{
  (void)open(std::move(db_path));
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
  sqlite3* raw = open_database(path_, /*foreign_keys=*/false, error);
  if (raw == nullptr) {
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

namespace {

/// Schema versions (PRAGMA user_version):
///   0  rows written before the mtime fix: mtime_ns was the raw tick count of
///      std::filesystem::file_time_type (implementation-defined epoch, 2174 on
///      libstdc++) - not comparable to anything else;
///   1  mtime_ns is nanoseconds since the Unix epoch (st_mtim).
constexpr int kSchemaVersion = 1;

int read_user_version(sqlite3* db)
{
  int v = 0;
  sqlite3_exec(
      db, "PRAGMA user_version",
      [](void* ctx, int n, char** vals, char**) -> int {
        if (n > 0 && vals[0] != nullptr) {
          *static_cast<int*>(ctx) = std::atoi(vals[0]);
        }
        return 0;
      },
      &v, nullptr);
  return v;
}

} // namespace

bool ChecksumStore::ensure_schema(std::string* error)
{
  auto* db = static_cast<sqlite3*>(db_);
  char* err = nullptr;
  auto fail = [&](const char* fallback) {
    if (error) {
      *error = err ? err : fallback;
    }
    if (err) {
      sqlite3_free(err);
      err = nullptr;
    }
    return false;
  };
  if (sqlite3_exec(db, kSchema, nullptr, nullptr, &err) != SQLITE_OK) {
    return fail("schema failed");
  }

  // Migrate old rows once. IMMEDIATE + re-reading the version inside the
  // transaction keeps two processes opening the same old database from both
  // applying the offset.
  if (read_user_version(db) < kSchemaVersion) {
    if (sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, &err) != SQLITE_OK) {
      return fail("cannot start schema migration");
    }
    if (read_user_version(db) < kSchemaVersion) {
      const std::int64_t offset = legacy_file_clock_offset_ns();
      // offset == 0 would also be right for a clock whose epoch is the Unix
      // epoch; for non-nanosecond clocks legacy_file_clock_offset_ns() cannot
      // convert and the stale rows are simply re-hashed on demand.
      const std::string sql =
          "UPDATE checksums SET mtime_ns = mtime_ns + " + std::to_string(offset) +
          " WHERE mtime_ns IS NOT NULL; PRAGMA user_version=" + std::to_string(kSchemaVersion) + ";";
      if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        return fail("schema migration failed");
      }
    }
    if (sqlite3_exec(db, "COMMIT", nullptr, nullptr, &err) != SQLITE_OK) {
      return fail("cannot commit schema migration");
    }
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

bool ChecksumStore::put(std::string_view path_key, const FileDigests& digests,
                        std::string* error)
{
  if (db_ == nullptr) {
    if (error != nullptr) {
      *error = "checksum store not open";
    }
    return false;
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
    return report_db_error(error, "prepare put");
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
  const int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE ? true : report_db_error(error, "put");
}

bool ChecksumStore::remove(std::string_view path_key, std::string* error)
{
  if (db_ == nullptr) {
    if (error != nullptr) {
      *error = "checksum store not open";
    }
    return false;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql = "DELETE FROM checksums WHERE path = ?1";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return report_db_error(error, "prepare remove");
  }
  sqlite3_bind_text(stmt, 1, path_key.data(), static_cast<int>(path_key.size()), SQLITE_TRANSIENT);
  const int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE ? true : report_db_error(error, "remove");
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
                      HashError* error, const HashOptions& hash_options)
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
  if (const auto stamp = stat_stamp(path)) {
    mtime_ns = stamp->mtime_ns;  // Unix nanoseconds
  }

  if (!refresh) {
    if (auto hit = get_if_valid(path_key, size, mtime_ns)) {
      return hit;
    }
  }

  auto digests = hash_file(path, hash_options, error);
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


std::string ChecksumStore::quick_key(std::string_view path_key)
{
  return std::string("quick:") + std::string(path_key);
}

std::optional<FileDigests> ChecksumStore::get_quick(std::string_view path_key) const
{
  return get(quick_key(path_key));
}

bool ChecksumStore::put_quick(std::string_view path_key, const FileDigests& digests,
                              std::string* error)
{
  return put(quick_key(path_key), digests, error);
}

bool ChecksumStore::has_full(std::string_view path_key) const
{
  auto d = get(path_key);
  return d.has_value() && d->sha256_hex.size() == 64;
}

bool ChecksumStore::has_quick(std::string_view path_key) const
{
  auto d = get_quick(path_key);
  return d.has_value() && d->sha256_hex.size() == 64;
}

} // namespace dirtoo::hash

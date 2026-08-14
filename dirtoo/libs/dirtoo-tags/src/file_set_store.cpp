// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/sets/file_set_store.hpp"

#include <sqlite3.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <random>
#include <sstream>

namespace dirtoo::sets {
namespace {

constexpr const char* kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS file_sets (
  id TEXT PRIMARY KEY NOT NULL,
  label TEXT NOT NULL DEFAULT '',
  color TEXT NOT NULL DEFAULT '',
  created_at INTEGER NOT NULL,
  updated_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS file_set_members (
  set_id TEXT NOT NULL REFERENCES file_sets(id) ON DELETE CASCADE,
  path_key TEXT NOT NULL,
  sha256 TEXT NOT NULL DEFAULT '',
  PRIMARY KEY (set_id, path_key)
);
CREATE INDEX IF NOT EXISTS idx_file_set_members_path ON file_set_members(path_key);
CREATE INDEX IF NOT EXISTS idx_file_set_members_sha ON file_set_members(sha256);
)SQL";

std::int64_t now_unix()
{
  return static_cast<std::int64_t>(
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

std::string column_text(sqlite3_stmt* stmt, int col)
{
  const auto* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
  return p != nullptr ? std::string{p} : std::string{};
}

/// Bind TEXT; empty string_view often has data()==nullptr, which SQLite treats as NULL.
void bind_text(sqlite3_stmt* stmt, int idx, std::string_view sv)
{
  const char* p = sv.empty() ? "" : sv.data();
  sqlite3_bind_text(stmt, idx, p, static_cast<int>(sv.size()), SQLITE_TRANSIENT);
}

FileSet row_to_set(sqlite3_stmt* stmt, bool with_count)
{
  FileSet s;
  s.id = column_text(stmt, 0);
  s.label = column_text(stmt, 1);
  s.color = column_text(stmt, 2);
  s.created_at = sqlite3_column_int64(stmt, 3);
  s.updated_at = sqlite3_column_int64(stmt, 4);
  if (with_count) {
    s.member_count = sqlite3_column_int64(stmt, 5);
  }
  return s;
}

} // namespace

std::string FileSetStore::new_id()
{
  // UUID-like 32 hex chars (version-4 style randomness; not RFC-strict).
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<int> dist(0, 15);
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(32);
  for (int i = 0; i < 32; ++i) {
    out[static_cast<std::size_t>(i)] = kHex[dist(rng)];
  }
  return out;
}

FileSetStore::FileSetStore(std::filesystem::path db_path)
{
  (void)open(std::move(db_path));
}

FileSetStore::~FileSetStore()
{
  close();
}

FileSetStore::FileSetStore(FileSetStore&& other) noexcept
    : db_(other.db_)
    , path_(std::move(other.path_))
{
  other.db_ = nullptr;
}

FileSetStore& FileSetStore::operator=(FileSetStore&& other) noexcept
{
  if (this != &other) {
    close();
    db_ = other.db_;
    path_ = std::move(other.path_);
    other.db_ = nullptr;
  }
  return *this;
}

std::filesystem::path FileSetStore::default_path()
{
  const char* data = std::getenv("XDG_DATA_HOME");
  std::filesystem::path dir;
  if (data != nullptr && data[0] != '\0') {
    dir = std::filesystem::path{data} / "dirtoo";
  } else {
    const char* home = std::getenv("HOME");
    dir = (home != nullptr) ? std::filesystem::path{home} / ".local" / "share" / "dirtoo"
                            : std::filesystem::path{"."} / "dirtoo-data";
  }
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir / "file_sets.sqlite";
}

bool FileSetStore::open(std::filesystem::path db_path, std::string* error)
{
  close();
  path_ = std::move(db_path);
  if (path_.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
  }
  sqlite3* raw = nullptr;
  if (sqlite3_open(path_.string().c_str(), &raw) != SQLITE_OK) {
    if (error != nullptr) {
      error->assign(raw != nullptr ? sqlite3_errmsg(raw) : "sqlite3_open failed");
    }
    if (raw != nullptr) {
      sqlite3_close(raw);
    }
    return false;
  }
  db_ = raw;
  sqlite3_exec(static_cast<sqlite3*>(db_), "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
  sqlite3_exec(static_cast<sqlite3*>(db_), "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
  if (!ensure_schema(error)) {
    close();
    return false;
  }
  return true;
}

void FileSetStore::close()
{
  if (db_ != nullptr) {
    sqlite3_close(static_cast<sqlite3*>(db_));
    db_ = nullptr;
  }
}

bool FileSetStore::ensure_schema(std::string* error)
{
  char* errmsg = nullptr;
  if (sqlite3_exec(static_cast<sqlite3*>(db_), kSchema, nullptr, nullptr, &errmsg) != SQLITE_OK) {
    if (error != nullptr) {
      error->assign(errmsg != nullptr ? errmsg : "schema failed");
    }
    sqlite3_free(errmsg);
    return false;
  }
  return true;
}

std::optional<FileSet>
FileSetStore::create_set(std::string_view label, std::string_view color, std::string* error)
{
  if (db_ == nullptr) {
    if (error != nullptr) {
      *error = "store not open";
    }
    return std::nullopt;
  }
  const std::string id = new_id();
  const auto now = now_unix();
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "INSERT INTO file_sets (id, label, color, created_at, updated_at) VALUES (?1,?2,?3,?4,?5)";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    if (error != nullptr) {
      *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
  bind_text(stmt, 2, label);
  bind_text(stmt, 3, color);
  sqlite3_bind_int64(stmt, 4, now);
  sqlite3_bind_int64(stmt, 5, now);
  const int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    if (error != nullptr) {
      *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return std::nullopt;
  }
  FileSet s;
  s.id = id;
  s.label = std::string(label);
  s.color = std::string(color);
  s.created_at = now;
  s.updated_at = now;
  s.member_count = 0;
  return s;
}

std::optional<FileSet> FileSetStore::get_set(std::string_view set_id) const
{
  if (db_ == nullptr) {
    return std::nullopt;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "SELECT s.id, s.label, s.color, s.created_at, s.updated_at, "
      "  (SELECT COUNT(*) FROM file_set_members m WHERE m.set_id = s.id) "
      "FROM file_sets s WHERE s.id = ?1";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  bind_text(stmt, 1, set_id);
  std::optional<FileSet> out;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    out = row_to_set(stmt, true);
  }
  sqlite3_finalize(stmt);
  return out;
}

std::vector<FileSet> FileSetStore::list_sets() const
{
  std::vector<FileSet> out;
  if (db_ == nullptr) {
    return out;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "SELECT s.id, s.label, s.color, s.created_at, s.updated_at, "
      "  (SELECT COUNT(*) FROM file_set_members m WHERE m.set_id = s.id) "
      "FROM file_sets s ORDER BY s.updated_at DESC, s.created_at DESC";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return out;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    out.push_back(row_to_set(stmt, true));
  }
  sqlite3_finalize(stmt);
  return out;
}

bool FileSetStore::set_label(std::string_view set_id, std::string_view label, std::string* error)
{
  if (db_ == nullptr) {
    if (error != nullptr) {
      *error = "store not open";
    }
    return false;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "UPDATE file_sets SET label = ?2, updated_at = ?3 WHERE id = ?1";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    if (error != nullptr) {
      *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return false;
  }
  bind_text(stmt, 1, set_id);
  bind_text(stmt, 2, label);
  sqlite3_bind_int64(stmt, 3, now_unix());
  const int rc = sqlite3_step(stmt);
  const int changes = sqlite3_changes(static_cast<sqlite3*>(db_));
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE || changes == 0) {
    if (error != nullptr) {
      *error = (changes == 0) ? "set not found" : sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return false;
  }
  return true;
}

bool FileSetStore::set_color(std::string_view set_id, std::string_view color, std::string* error)
{
  if (db_ == nullptr) {
    if (error != nullptr) {
      *error = "store not open";
    }
    return false;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "UPDATE file_sets SET color = ?2, updated_at = ?3 WHERE id = ?1";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    if (error != nullptr) {
      *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return false;
  }
  bind_text(stmt, 1, set_id);
  bind_text(stmt, 2, color);
  sqlite3_bind_int64(stmt, 3, now_unix());
  const int rc = sqlite3_step(stmt);
  const int changes = sqlite3_changes(static_cast<sqlite3*>(db_));
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE || changes == 0) {
    if (error != nullptr) {
      *error = (changes == 0) ? "set not found" : sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return false;
  }
  return true;
}

bool FileSetStore::delete_set(std::string_view set_id, std::string* error)
{
  if (db_ == nullptr) {
    if (error != nullptr) {
      *error = "store not open";
    }
    return false;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql = "DELETE FROM file_sets WHERE id = ?1";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    if (error != nullptr) {
      *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return false;
  }
  bind_text(stmt, 1, set_id);
  const int rc = sqlite3_step(stmt);
  const int changes = sqlite3_changes(static_cast<sqlite3*>(db_));
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    if (error != nullptr) {
      *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return false;
  }
  if (changes == 0) {
    if (error != nullptr) {
      *error = "set not found";
    }
    return false;
  }
  return true;
}

bool FileSetStore::add_member(std::string_view set_id, std::string_view path_key,
                              std::string_view sha256, std::string* error)
{
  if (db_ == nullptr) {
    if (error != nullptr) {
      *error = "store not open";
    }
    return false;
  }
  if (path_key.empty()) {
    if (error != nullptr) {
      *error = "empty path_key";
    }
    return false;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "INSERT INTO file_set_members (set_id, path_key, sha256) VALUES (?1,?2,?3) "
      "ON CONFLICT(set_id, path_key) DO UPDATE SET sha256 = excluded.sha256";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    if (error != nullptr) {
      *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return false;
  }
  bind_text(stmt, 1, set_id);
  bind_text(stmt, 2, path_key);
  bind_text(stmt, 3, sha256);
  const int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    if (error != nullptr) {
      *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return false;
  }
  // Touch updated_at.
  sqlite3_stmt* touch = nullptr;
  constexpr const char* tsql = "UPDATE file_sets SET updated_at = ?2 WHERE id = ?1";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), tsql, -1, &touch, nullptr) == SQLITE_OK) {
    bind_text(touch, 1, set_id);
    sqlite3_bind_int64(touch, 2, now_unix());
    sqlite3_step(touch);
    sqlite3_finalize(touch);
  }
  return true;
}

bool FileSetStore::remove_member(std::string_view set_id, std::string_view path_key,
                                 std::string* error)
{
  if (db_ == nullptr) {
    if (error != nullptr) {
      *error = "store not open";
    }
    return false;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "DELETE FROM file_set_members WHERE set_id = ?1 AND path_key = ?2";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    if (error != nullptr) {
      *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return false;
  }
  bind_text(stmt, 1, set_id);
  bind_text(stmt, 2, path_key);
  const int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    if (error != nullptr) {
      *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return false;
  }
  return true;
}

int FileSetStore::add_members(std::string_view set_id, const std::vector<std::string>& path_keys,
                              std::string* error)
{
  int n = 0;
  for (const auto& pk : path_keys) {
    if (pk.empty()) {
      continue;
    }
    const bool already = contains(set_id, pk);
    if (!add_member(set_id, pk, {}, error)) {
      return n;
    }
    if (!already) {
      ++n;
    }
  }
  return n;
}

std::vector<FileSetMember> FileSetStore::members(std::string_view set_id) const
{
  std::vector<FileSetMember> out;
  if (db_ == nullptr) {
    return out;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "SELECT path_key, sha256 FROM file_set_members WHERE set_id = ?1 ORDER BY path_key";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return out;
  }
  bind_text(stmt, 1, set_id);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    FileSetMember m;
    m.path_key = column_text(stmt, 0);
    m.sha256 = column_text(stmt, 1);
    out.push_back(std::move(m));
  }
  sqlite3_finalize(stmt);
  return out;
}

std::int64_t FileSetStore::member_count(std::string_view set_id) const
{
  if (db_ == nullptr) {
    return 0;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "SELECT COUNT(*) FROM file_set_members WHERE set_id = ?1";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return 0;
  }
  bind_text(stmt, 1, set_id);
  std::int64_t n = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    n = sqlite3_column_int64(stmt, 0);
  }
  sqlite3_finalize(stmt);
  return n;
}

std::vector<FileSet> FileSetStore::sets_for_path(std::string_view path_key) const
{
  std::vector<FileSet> out;
  if (db_ == nullptr || path_key.empty()) {
    return out;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "SELECT s.id, s.label, s.color, s.created_at, s.updated_at, "
      "  (SELECT COUNT(*) FROM file_set_members m2 WHERE m2.set_id = s.id) "
      "FROM file_sets s "
      "INNER JOIN file_set_members m ON m.set_id = s.id "
      "WHERE m.path_key = ?1 "
      "ORDER BY s.updated_at DESC";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return out;
  }
  bind_text(stmt, 1, path_key);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    out.push_back(row_to_set(stmt, true));
  }
  sqlite3_finalize(stmt);
  return out;
}

bool FileSetStore::contains(std::string_view set_id, std::string_view path_key) const
{
  if (db_ == nullptr) {
    return false;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "SELECT 1 FROM file_set_members WHERE set_id = ?1 AND path_key = ?2 LIMIT 1";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  bind_text(stmt, 1, set_id);
  bind_text(stmt, 2, path_key);
  const bool hit = sqlite3_step(stmt) == SQLITE_ROW;
  sqlite3_finalize(stmt);
  return hit;
}

} // namespace dirtoo::sets

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/tree/fs_tree_size_store.hpp"

#include <sqlite3.h>

#include <cstdlib>
#include <ctime>
#include <system_error>

namespace dirtoo::tree {
namespace {

constexpr const char* kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS entry_sizes (
  path TEXT PRIMARY KEY NOT NULL,
  total_size INTEGER NOT NULL,
  updated_at INTEGER NOT NULL
);
)SQL";

bool exec_sql(sqlite3* db, const char* sql)
{
  char* err = nullptr;
  const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
  if (err != nullptr) {
    sqlite3_free(err);
  }
  return rc == SQLITE_OK;
}

} // namespace

FsTreeSizeStore::FsTreeSizeStore(std::filesystem::path db_path)
{
  (void)open(std::move(db_path));
}

FsTreeSizeStore::~FsTreeSizeStore()
{
  close();
}

FsTreeSizeStore::FsTreeSizeStore(FsTreeSizeStore&& other) noexcept
    : db_(other.db_)
    , path_(std::move(other.path_))
{
  other.db_ = nullptr;
}

FsTreeSizeStore& FsTreeSizeStore::operator=(FsTreeSizeStore&& other) noexcept
{
  if (this != &other) {
    close();
    db_ = other.db_;
    path_ = std::move(other.path_);
    other.db_ = nullptr;
  }
  return *this;
}

std::filesystem::path FsTreeSizeStore::default_db_path()
{
  const char* xdg = std::getenv("XDG_CACHE_HOME");
  std::filesystem::path base;
  if (xdg != nullptr && xdg[0] != '\0') {
    base = xdg;
  } else {
    const char* home = std::getenv("HOME");
    base = (home != nullptr) ? std::filesystem::path{home} / ".cache" : std::filesystem::path{"."};
  }
  return base / "dirtoo" / "fstree-sizes.sqlite";
}

bool FsTreeSizeStore::open(std::filesystem::path db_path)
{
  close();
  path_ = std::move(db_path);
  std::error_code ec;
  std::filesystem::create_directories(path_.parent_path(), ec);
  sqlite3* raw = nullptr;
  if (sqlite3_open(path_.string().c_str(), &raw) != SQLITE_OK) {
    if (raw != nullptr) {
      sqlite3_close(raw);
    }
    path_.clear();
    return false;
  }
  db_ = raw;
  exec_sql(static_cast<sqlite3*>(db_), "PRAGMA journal_mode=WAL;");
  exec_sql(static_cast<sqlite3*>(db_), "PRAGMA synchronous=NORMAL;");
  if (!exec_sql(static_cast<sqlite3*>(db_), kSchema)) {
    close();
    return false;
  }
  return true;
}

void FsTreeSizeStore::close()
{
  if (db_ != nullptr) {
    sqlite3_close(static_cast<sqlite3*>(db_));
    db_ = nullptr;
  }
}

void FsTreeSizeStore::upsert(std::string_view path_key, std::uint64_t total_size)
{
  if (db_ == nullptr || path_key.empty()) {
    return;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "INSERT INTO entry_sizes(path, total_size, updated_at) VALUES(?,?,?) "
      "ON CONFLICT(path) DO UPDATE SET total_size=excluded.total_size, updated_at=excluded.updated_at;";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  sqlite3_bind_text(stmt, 1, path_key.data(), static_cast<int>(path_key.size()), SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(total_size));
  sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(std::time(nullptr)));
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

std::optional<std::uint64_t> FsTreeSizeStore::get(std::string_view path_key) const
{
  if (db_ == nullptr || path_key.empty()) {
    return std::nullopt;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql = "SELECT total_size FROM entry_sizes WHERE path=? LIMIT 1;";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, path_key.data(), static_cast<int>(path_key.size()), SQLITE_TRANSIENT);
  std::optional<std::uint64_t> out;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    out = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
  }
  sqlite3_finalize(stmt);
  return out;
}

void FsTreeSizeStore::load_all(std::unordered_map<std::string, std::uint64_t>& out) const
{
  out.clear();
  if (db_ == nullptr) {
    return;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql = "SELECT path, total_size FROM entry_sizes;";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const auto* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (p == nullptr) {
      continue;
    }
    out.emplace(p, static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 1)));
  }
  sqlite3_finalize(stmt);
}

void FsTreeSizeStore::erase(std::string_view path_key)
{
  if (db_ == nullptr || path_key.empty()) {
    return;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql = "DELETE FROM entry_sizes WHERE path=?;";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  sqlite3_bind_text(stmt, 1, path_key.data(), static_cast<int>(path_key.size()), SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void FsTreeSizeStore::erase_under(std::string_view path_prefix)
{
  if (db_ == nullptr || path_prefix.empty()) {
    return;
  }
  // Exact key + children with '/' boundary.
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "DELETE FROM entry_sizes WHERE path=? OR path LIKE ? ESCAPE '\\';";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  const std::string exact{path_prefix};
  std::string like = exact;
  // Escape LIKE metacharacters in prefix.
  std::string escaped;
  for (char c : exact) {
    if (c == '%' || c == '_' || c == '\\') {
      escaped += '\\';
    }
    escaped += c;
  }
  like = escaped + "/%";
  sqlite3_bind_text(stmt, 1, exact.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, like.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void FsTreeSizeStore::clear()
{
  if (db_ == nullptr) {
    return;
  }
  exec_sql(static_cast<sqlite3*>(db_), "DELETE FROM entry_sizes;");
}

} // namespace dirtoo::tree

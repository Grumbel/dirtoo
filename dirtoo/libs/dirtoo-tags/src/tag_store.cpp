// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/tags/tag_store.hpp"

#include "dirtoo/hash/checksum_store.hpp"

#include <sqlite3.h>

#include <cstdlib>
#include <ctime>

namespace dirtoo::tags {
namespace {

constexpr const char* kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS tag_defs (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  label TEXT,
  color TEXT,
  badge TEXT,
  created INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS files (
  id INTEGER PRIMARY KEY,
  sha256 TEXT NOT NULL UNIQUE
);
CREATE TABLE IF NOT EXISTS paths (
  path TEXT PRIMARY KEY NOT NULL,
  file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
  last_seen INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS file_tags (
  file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
  tag_id INTEGER NOT NULL REFERENCES tag_defs(id) ON DELETE CASCADE,
  tagged_at INTEGER NOT NULL,
  PRIMARY KEY (file_id, tag_id)
);
CREATE INDEX IF NOT EXISTS idx_paths_file ON paths(file_id);
CREATE INDEX IF NOT EXISTS idx_file_tags_tag ON file_tags(tag_id);
)SQL";

} // namespace

TagStore::~TagStore()
{
  close();
}

TagStore::TagStore(TagStore&& other) noexcept
    : db_(other.db_)
    , path_(std::move(other.path_))
{
  other.db_ = nullptr;
}

TagStore& TagStore::operator=(TagStore&& other) noexcept
{
  if (this != &other) {
    close();
    db_ = other.db_;
    path_ = std::move(other.path_);
    other.db_ = nullptr;
  }
  return *this;
}

std::filesystem::path TagStore::default_path()
{
  const char* data = std::getenv("XDG_DATA_HOME");
  std::filesystem::path dir;
  if (data != nullptr && data[0] != '\0') {
    dir = std::filesystem::path{data} / "dirtoo";
  } else {
    const char* home = std::getenv("HOME");
    dir = std::filesystem::path{home != nullptr ? home : "."} / ".local" / "share" / "dirtoo";
  }
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir / "tags.sqlite";
}

bool TagStore::open(std::filesystem::path db_path, std::string* error)
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
  sqlite3_exec(raw, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr);
  db_ = raw;
  if (!ensure_schema(error)) {
    close();
    return false;
  }
  return true;
}

void TagStore::close()
{
  if (db_ != nullptr) {
    sqlite3_close(static_cast<sqlite3*>(db_));
    db_ = nullptr;
  }
}

bool TagStore::ensure_schema(std::string* error)
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

std::optional<TagDef> TagStore::ensure_tag(std::string_view name, std::string* error)
{
  const std::string norm = normalize_tag_name(name);
  if (norm.empty()) {
    if (error) {
      *error = "invalid tag name";
    }
    return std::nullopt;
  }
  if (auto existing = get_tag(norm)) {
    return existing;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "INSERT INTO tag_defs(name, label, color, badge, created) VALUES(?1,?2,'','',?3)";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    if (error) {
      *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return std::nullopt;
  }
  const auto now = static_cast<std::int64_t>(std::time(nullptr));
  sqlite3_bind_text(stmt, 1, norm.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, norm.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, now);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    if (error) {
      *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
  }
  sqlite3_finalize(stmt);
  return get_tag(norm);
}

std::optional<TagDef> TagStore::get_tag(std::string_view name) const
{
  if (db_ == nullptr) {
    return std::nullopt;
  }
  const std::string norm = normalize_tag_name(name);
  if (norm.empty()) {
    return std::nullopt;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "SELECT id, name, label, color, badge FROM tag_defs WHERE name = ?1";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, norm.c_str(), -1, SQLITE_TRANSIENT);
  std::optional<TagDef> out;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    TagDef d;
    d.id = sqlite3_column_int64(stmt, 0);
    d.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    if (const char* lab = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))) {
      d.label = lab;
    }
    if (const char* col = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))) {
      d.color = col;
    }
    if (const char* badge = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4))) {
      d.badge = badge;
    }
    out = std::move(d);
  }
  sqlite3_finalize(stmt);
  return out;
}

std::vector<TagDef> TagStore::list_tags() const
{
  std::vector<TagDef> out;
  if (db_ == nullptr) {
    return out;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "SELECT id, name, label, color, badge FROM tag_defs ORDER BY name";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return out;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    TagDef d;
    d.id = sqlite3_column_int64(stmt, 0);
    d.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    if (const char* lab = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))) {
      d.label = lab;
    }
    if (const char* col = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))) {
      d.color = col;
    }
    if (const char* badge = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4))) {
      d.badge = badge;
    }
    out.push_back(std::move(d));
  }
  sqlite3_finalize(stmt);
  return out;
}

bool TagStore::set_tag_meta(std::string_view name, std::optional<std::string> label,
                            std::optional<std::string> color, std::optional<std::string> badge,
                            std::string* error)
{
  auto def = ensure_tag(name, error);
  if (!def) {
    return false;
  }
  if (label) {
    def->label = *label;
  }
  if (color) {
    def->color = *color;
  }
  if (badge) {
    def->badge = *badge;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "UPDATE tag_defs SET label = ?1, color = ?2, badge = ?3 WHERE id = ?4";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    if (error) {
      *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return false;
  }
  sqlite3_bind_text(stmt, 1, def->label.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, def->color.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, def->badge.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 4, def->id);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  if (!ok && error) {
    *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
  }
  sqlite3_finalize(stmt);
  return ok;
}

std::optional<std::int64_t>
TagStore::ensure_file_sha256(std::string_view sha256, std::string_view path_key, std::string* error)
{
  if (db_ == nullptr || sha256.size() != 64) {
    if (error) {
      *error = "invalid sha256";
    }
    return std::nullopt;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sel = "SELECT id FROM files WHERE sha256 = ?1";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sel, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, sha256.data(), static_cast<int>(sha256.size()), SQLITE_TRANSIENT);
  std::optional<std::int64_t> id;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    id = sqlite3_column_int64(stmt, 0);
  }
  sqlite3_finalize(stmt);

  if (!id) {
    constexpr const char* ins = "INSERT INTO files(sha256) VALUES(?1)";
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), ins, -1, &stmt, nullptr) != SQLITE_OK) {
      if (error) {
        *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
      }
      return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, sha256.data(), static_cast<int>(sha256.size()), SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
      if (error) {
        *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
      }
      sqlite3_finalize(stmt);
      return std::nullopt;
    }
    id = static_cast<std::int64_t>(sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_)));
    sqlite3_finalize(stmt);
  }

  if (!path_key.empty() && id) {
    constexpr const char* ups =
        "INSERT INTO paths(path, file_id, last_seen) VALUES(?1,?2,?3) "
        "ON CONFLICT(path) DO UPDATE SET file_id=excluded.file_id, last_seen=excluded.last_seen";
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), ups, -1, &stmt, nullptr) == SQLITE_OK) {
      const auto now = static_cast<std::int64_t>(std::time(nullptr));
      sqlite3_bind_text(stmt, 1, path_key.data(), static_cast<int>(path_key.size()),
                        SQLITE_TRANSIENT);
      sqlite3_bind_int64(stmt, 2, *id);
      sqlite3_bind_int64(stmt, 3, now);
      sqlite3_step(stmt);
      sqlite3_finalize(stmt);
    }
  }
  return id;
}

std::optional<std::int64_t>
TagStore::resolve_path(const dirtoo::hash::ChecksumStore& checksums, std::string_view path_key,
                       std::string* error)
{
  auto digests = checksums.get(std::string(path_key));
  if (!digests) {
    // try absolute normalization already expected from caller
    if (error) {
      *error = "checksum unknown; run dt-checksum first";
    }
    return std::nullopt;
  }
  if (digests->sha256_hex.size() != 64) {
    if (error) {
      *error = "cached checksum missing sha256";
    }
    return std::nullopt;
  }
  return ensure_file_sha256(digests->sha256_hex, path_key, error);
}

bool TagStore::add_tag_to_file(std::int64_t file_id, std::string_view tag_name, std::string* error)
{
  auto def = ensure_tag(tag_name, error);
  if (!def) {
    return false;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "INSERT OR IGNORE INTO file_tags(file_id, tag_id, tagged_at) VALUES(?1,?2,?3)";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    if (error) {
      *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
    }
    return false;
  }
  const auto now = static_cast<std::int64_t>(std::time(nullptr));
  sqlite3_bind_int64(stmt, 1, file_id);
  sqlite3_bind_int64(stmt, 2, def->id);
  sqlite3_bind_int64(stmt, 3, now);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  if (!ok && error) {
    *error = sqlite3_errmsg(static_cast<sqlite3*>(db_));
  }
  sqlite3_finalize(stmt);
  return ok;
}

bool TagStore::remove_tag_from_file(std::int64_t file_id, std::string_view tag_name,
                                    std::string* error)
{
  auto def = get_tag(tag_name);
  if (!def) {
    if (error) {
      *error = "unknown tag";
    }
    return false;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql = "DELETE FROM file_tags WHERE file_id = ?1 AND tag_id = ?2";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_int64(stmt, 1, file_id);
  sqlite3_bind_int64(stmt, 2, def->id);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

std::vector<std::string> TagStore::tags_for_file(std::int64_t file_id) const
{
  std::vector<std::string> out;
  if (db_ == nullptr) {
    return out;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "SELECT t.name FROM file_tags ft JOIN tag_defs t ON t.id = ft.tag_id "
      "WHERE ft.file_id = ?1 ORDER BY t.name";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return out;
  }
  sqlite3_bind_int64(stmt, 1, file_id);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    out.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
  }
  sqlite3_finalize(stmt);
  return out;
}

std::vector<std::string> TagStore::tags_for_sha256(std::string_view sha256) const
{
  if (db_ == nullptr) {
    return {};
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql = "SELECT id FROM files WHERE sha256 = ?1";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return {};
  }
  sqlite3_bind_text(stmt, 1, sha256.data(), static_cast<int>(sha256.size()), SQLITE_TRANSIENT);
  std::optional<std::int64_t> id;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    id = sqlite3_column_int64(stmt, 0);
  }
  sqlite3_finalize(stmt);
  if (!id) {
    return {};
  }
  return tags_for_file(*id);
}

std::vector<std::string> TagStore::tags_for_path(const dirtoo::hash::ChecksumStore& checksums,
                                                std::string_view path_key) const
{
  std::string err;
  // resolve_path is non-const (may upsert); use const path via checksums only
  auto digests = checksums.get(std::string(path_key));
  if (!digests) {
    return {};
  }
  return tags_for_sha256(digests->sha256_hex);
}

std::vector<TaggedFile> TagStore::files_for_tag(std::string_view tag_name) const
{
  std::vector<TaggedFile> out;
  auto def = get_tag(tag_name);
  if (!def || db_ == nullptr) {
    return out;
  }
  sqlite3_stmt* stmt = nullptr;
  constexpr const char* sql =
      "SELECT f.id, f.sha256 FROM file_tags ft JOIN files f ON f.id = ft.file_id "
      "WHERE ft.tag_id = ?1 ORDER BY f.sha256";
  if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return out;
  }
  sqlite3_bind_int64(stmt, 1, def->id);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    TaggedFile tf;
    tf.file_id = sqlite3_column_int64(stmt, 0);
    tf.sha256 = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    out.push_back(std::move(tf));
  }
  sqlite3_finalize(stmt);

  for (auto& tf : out) {
    sqlite3_stmt* pstmt = nullptr;
    constexpr const char* psql = "SELECT path FROM paths WHERE file_id = ?1 ORDER BY path";
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), psql, -1, &pstmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_int64(pstmt, 1, tf.file_id);
      while (sqlite3_step(pstmt) == SQLITE_ROW) {
        tf.paths.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(pstmt, 0)));
      }
      sqlite3_finalize(pstmt);
    }
    tf.tags = tags_for_file(tf.file_id);
  }
  return out;
}

} // namespace dirtoo::tags

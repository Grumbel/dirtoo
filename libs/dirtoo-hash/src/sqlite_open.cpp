// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/hash/sqlite_open.hpp"

#include <sqlite3.h>

#include <cstdio>
#include <system_error>

namespace dirtoo::hash {
namespace {

/// Run a PRAGMA and return the first column of its first row ("" if none).
std::string pragma_value(sqlite3* db, const char* sql)
{
  std::string out;
  sqlite3_exec(
      db, sql,
      [](void* ctx, int ncols, char** vals, char**) -> int {
        auto* o = static_cast<std::string*>(ctx);
        if (o->empty() && ncols > 0 && vals[0] != nullptr) {
          *o = vals[0];
        }
        return 0;
      },
      &out, nullptr);
  return out;
}

} // namespace

sqlite3* open_database(const std::filesystem::path& path, bool foreign_keys, std::string* error)
{
  auto fail = [&](const std::string& msg) -> sqlite3* {
    if (error != nullptr) {
      *error = msg;
    }
    return nullptr;
  };

  if (auto parent = path.parent_path(); !parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
  }

  sqlite3* raw = nullptr;
  if (sqlite3_open(path.string().c_str(), &raw) != SQLITE_OK) {
    const std::string msg = raw != nullptr ? sqlite3_errmsg(raw) : "sqlite3_open failed";
    if (raw != nullptr) {
      sqlite3_close(raw);
    }
    return fail(msg);
  }

  // SQLite gives the -wal/-shm files the same mode as the main file.
  std::error_code perm_ec;
  std::filesystem::permissions(path,
                               std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::replace, perm_ec);
  if (perm_ec) {
    std::fprintf(stderr, "dirtoo: cannot restrict permissions of %s: %s\n", path.c_str(),
                 perm_ec.message().c_str());
  }

  sqlite3_busy_timeout(raw, 5000);

  if (foreign_keys) {
    sqlite3_exec(raw, "PRAGMA foreign_keys=ON", nullptr, nullptr, nullptr);
    if (pragma_value(raw, "PRAGMA foreign_keys") != "1") {
      sqlite3_close(raw);
      return fail("SQLite foreign key enforcement is unavailable");
    }
  }

  const std::string mode = pragma_value(raw, "PRAGMA journal_mode=WAL");
  if (mode != "wal") {
    std::fprintf(stderr, "dirtoo: %s: WAL journal not available (got '%s')\n", path.c_str(),
                 mode.c_str());
  }
  sqlite3_exec(raw, "PRAGMA synchronous=NORMAL", nullptr, nullptr, nullptr);
  return raw;
}

} // namespace dirtoo::hash

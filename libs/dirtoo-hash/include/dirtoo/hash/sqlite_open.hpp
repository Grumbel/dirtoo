// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>

struct sqlite3;

namespace dirtoo::hash {

/// Open (creating if needed) one of dirtoo's SQLite databases with the
/// settings every store needs:
///  - parent directories are created and the file is made private (0600): the
///    databases list a user's files, hashes and tags;
///  - `busy_timeout` of 5 s, so concurrent windows/jobs wait instead of
///    failing at once with SQLITE_BUSY;
///  - WAL journal and `synchronous=NORMAL` (WAL can be refused on some network
///    filesystems; that is reported on stderr, not fatal);
///  - when @p foreign_keys is set, `foreign_keys=ON` and a check that it took
///    effect (without it ON DELETE CASCADE silently does nothing).
/// Returns nullptr and fills @p error on failure. The caller owns the handle
/// (sqlite3_close).
[[nodiscard]] sqlite3* open_database(const std::filesystem::path& path, bool foreign_keys,
                                     std::string* error);

} // namespace dirtoo::hash

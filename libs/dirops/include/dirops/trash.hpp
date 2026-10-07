// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirops/error.hpp"
#include "dirops/ops.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <sys/types.h>

/// Implementation of the freedesktop.org Trash specification 1.0
/// (https://specifications.freedesktop.org/trash-spec/1.0/).
///
/// - Files on the same filesystem as the home trash (`$XDG_DATA_HOME/Trash`)
///   go there; files on other filesystems go to a trash directory in the
///   top directory of that filesystem: `$topdir/.Trash/$uid` when `.Trash`
///   is a real (non-symlink) directory with the sticky bit, otherwise
///   `$topdir/.Trash-$uid`. A file is never copied across devices.
/// - Every trashed item is a pair: the item itself in `files/<name>` and a
///   `info/<name>.trashinfo` recording the original `Path` (percent-encoded;
///   absolute for the home trash, relative to the top directory for volume
///   trashes) and the `DeletionDate` (local time).
/// - Names are made unique atomically (the info file is created with
///   O_EXCL); the item is moved with rename(2) (RENAME_NOREPLACE where
///   available), so nothing is ever overwritten.
/// - The optional `directorysizes` cache is not maintained; it is validated
///   by readers against mtimes, so stale content is harmless, and it is
///   removed when the trash is emptied.
namespace dirops {

struct TrashOptions {
  /// Home trash directory; default `$XDG_DATA_HOME/Trash` or `~/.local/share/Trash`.
  std::optional<std::filesystem::path> home_trash;
  /// User id used for `.Trash/$uid` and `.Trash-$uid`; default geteuid().
  std::optional<uid_t> uid;
  /// Mount points searched for volume trash directories when listing or
  /// emptying; default: the mount points in /proc/self/mounts.
  std::optional<std::vector<std::filesystem::path>> mount_points;
  /// Use this as the volume top directory for files that are not on the home
  /// trash's filesystem, instead of walking up to the mount point. For tests.
  std::optional<std::filesystem::path> topdir_override;
};

struct TrashEntry {
  std::filesystem::path trash_dir;      ///< the trash directory holding it (has files/ and info/)
  std::filesystem::path topdir;         ///< volume top directory; empty for the home trash
  std::string name;                     ///< name inside files/ and info/ (without ".trashinfo")
  std::filesystem::path original_path;  ///< absolute original location
  std::string deletion_date;            ///< as stored, "YYYY-MM-DDThh:mm:ss"

  [[nodiscard]] std::filesystem::path stored_path() const { return trash_dir / "files" / name; }
  [[nodiscard]] std::filesystem::path info_path() const
  {
    return trash_dir / "info" / (name + ".trashinfo");
  }
};

/// The default home trash directory.
[[nodiscard]] std::filesystem::path home_trash_dir();

/// Move @p path to the trash (the item itself, symlinks are not followed).
/// Refuses empty/root/"."/".." paths, mount points, and anything already
/// inside a trash directory.
[[nodiscard]] std::expected<TrashEntry, Error>
trash_entry(const std::filesystem::path& path, const TrashOptions& trash = {});

/// Same, in the common OpResult shape: ItemResult{source = original path,
/// destination = path inside the trash}. Honours dry_run and is_cancelled.
[[nodiscard]] OpResult trash_path(const std::filesystem::path& path,
                                  const TrashOptions& trash = {}, const Options& options = {});

/// Every item in the home trash and in the volume trashes of the user.
/// Does file I/O on each mount point: worker threads only.
[[nodiscard]] std::vector<TrashEntry> list_trash(const TrashOptions& trash = {});

/// Move the item back to its original location (parents are created).
/// Fails with EEXIST instead of overwriting, then the item stays in the trash.
[[nodiscard]] OpResult restore_trash_entry(const TrashEntry& entry, const Options& options = {});

/// Permanently delete one trashed item and its info file.
[[nodiscard]] OpResult delete_trash_entry(const TrashEntry& entry, const Options& options = {});

/// Permanently delete everything in all of the user's trash directories
/// (including orphaned files/info and the directorysizes cache).
[[nodiscard]] OpResult empty_trash(const TrashOptions& trash = {}, const Options& options = {});

} // namespace dirops

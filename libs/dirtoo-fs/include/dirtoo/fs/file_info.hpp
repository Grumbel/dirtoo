// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#if !defined(_WIN32)
struct stat;
#endif

namespace dirtoo::fs {

class FileInfo {
public:
  [[nodiscard]] static FileInfo from_path(const std::filesystem::path& path);
  [[nodiscard]] static FileInfo from_location(const Location& location);
  /// Build from a directory_iterator entry using cached status (listing hot path).
  [[nodiscard]] static FileInfo from_directory_entry(const std::filesystem::directory_entry& entry);

  /// Virtual entry (e.g. archive member) that may not exist on the real FS.
  [[nodiscard]] static FileInfo synthetic(Location location, std::string display_name,
                                          bool is_directory, std::uint64_t size = 0);

  /// Set mtime from Unix epoch seconds (search worker already has this from dirent).
  /// No-op when sec <= 0 (the search worker passes -1 for "unknown"). Keeps the
  /// entry synthetic (no extra stat).
  void set_mtime_unix(std::int64_t sec);

  /// Set mtime from Unix epoch nanoseconds. Any value, including 0 and
  /// negative (before 1970), is a real time.
  void set_mtime_unix_ns(std::int64_t ns);

  [[nodiscard]] const Location& location() const noexcept { return location_; }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

  [[nodiscard]] std::string basename() const;
  [[nodiscard]] std::string extension() const;

  [[nodiscard]] std::uint64_t size() const noexcept { return size_; }
  /// The modification time, valid only if has_mtime(); otherwise it is the
  /// default file_time_type, which is NOT "no time" but a date in the year 2174.
  [[nodiscard]] std::filesystem::file_time_type mtime() const noexcept { return mtime_; }
  /// False for synthetic entries (archive members, ...) and failed stats.
  [[nodiscard]] bool has_mtime() const noexcept { return has_mtime_; }
  /// mtime as nanoseconds since the Unix epoch (st_mtim precision); nullopt if unknown.
  [[nodiscard]] std::optional<std::int64_t> mtime_unix_ns() const noexcept;

  /// POSIX atime / ctime / birth (creation) as system_clock time points.
  /// Zero epoch means "unknown / not available" (synthetic entries, failed stat).
  [[nodiscard]] std::chrono::system_clock::time_point atime() const noexcept { return atime_; }
  [[nodiscard]] std::chrono::system_clock::time_point ctime() const noexcept { return ctime_; }
  [[nodiscard]] std::chrono::system_clock::time_point birthtime() const noexcept { return birthtime_; }
  [[nodiscard]] bool has_atime() const noexcept { return has_atime_; }
  [[nodiscard]] bool has_ctime() const noexcept { return has_ctime_; }
  [[nodiscard]] bool has_birthtime() const noexcept { return has_birthtime_; }

  [[nodiscard]] bool is_directory() const noexcept { return is_directory_; }
  [[nodiscard]] bool is_regular_file() const noexcept { return is_regular_file_; }
  [[nodiscard]] bool is_symlink() const noexcept { return is_symlink_; }
  [[nodiscard]] bool is_synthetic() const noexcept { return is_synthetic_; }

  /// Link text as stored in the symlink (readlink); empty for non-symlinks.
  /// Captured when the entry is built (worker thread) so the GUI never has to
  /// readlink/stat to describe a link.
  [[nodiscard]] const std::filesystem::path& symlink_target() const noexcept
  {
    return symlink_target_;
  }
  /// Symlink whose target did not resolve when the entry was built.
  [[nodiscard]] bool is_broken_symlink() const noexcept { return is_broken_symlink_; }
  /// Symlink that resolved to a directory when the entry was built.
  [[nodiscard]] bool symlink_target_is_directory() const noexcept
  {
    return symlink_target_is_directory_;
  }

  [[nodiscard]] std::filesystem::perms permissions() const noexcept { return permissions_; }

  /// Owner / group from the lstat taken when the entry was built; `has_owner()`
  /// is false when no stat was available (then the ids are meaningless).
  [[nodiscard]] bool has_owner() const noexcept { return has_owner_; }
  [[nodiscard]] std::uint32_t owner_uid() const noexcept { return owner_uid_; }
  [[nodiscard]] std::uint32_t owner_gid() const noexcept { return owner_gid_; }

private:
  void fill_posix_times_from_path(const std::filesystem::path& path);
  void fill_symlink_target(const std::filesystem::path& path);
#if !defined(_WIN32)
  void apply_posix_stat(const struct stat& st);
#endif

  Location location_;
  std::filesystem::path path_;
  std::string display_name_;
  std::uint64_t size_ = 0;
  std::filesystem::file_time_type mtime_{};
  bool has_mtime_ = false;
  std::chrono::system_clock::time_point atime_{};
  std::chrono::system_clock::time_point ctime_{};
  std::chrono::system_clock::time_point birthtime_{};
  bool has_atime_ = false;
  bool has_ctime_ = false;
  bool has_birthtime_ = false;
  bool is_directory_ = false;
  bool is_regular_file_ = false;
  bool is_symlink_ = false;
  bool is_synthetic_ = false;
  bool is_broken_symlink_ = false;
  bool symlink_target_is_directory_ = false;
  std::filesystem::path symlink_target_;
  std::filesystem::perms permissions_{};
  bool has_owner_ = false;
  std::uint32_t owner_uid_ = 0;
  std::uint32_t owner_gid_ = 0;
};

/// List non-recursive directory entries. Hidden files included; caller filters.
[[nodiscard]] std::vector<FileInfo> list_directory(const Location& location);

} // namespace dirtoo::fs

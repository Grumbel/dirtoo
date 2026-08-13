// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace dirtoo::fs {

/// URI-like location.
///
/// Forms:
/// - file:  path is a normal filesystem path
/// - archive: path is the archive file; entry_ is the path inside the archive
/// - tag: path is the tag name (or comma-separated names); virtual collection
///
/// URL encoding for archives (Python-style, preferred):
///   file:///abs/path/to.zip//archive
///   file:///abs/path/to.zip//archive:inner/dir
/// Legacy JAR-inspired form still accepted by from_url:
///   archive:///abs/path/to.zip!/inner/dir
/// Tag collections:
///   tag://work
///   tag://game:doom
///   tag://foo,bar
class Location {
public:
  Location() = default;

  [[nodiscard]] static Location from_path(const std::filesystem::path& path);
  /// Like from_path but skips weakly_canonical — use for directory children when the
  /// parent is already absolute/normalized (listing hot path).
  [[nodiscard]] static Location from_path_unchecked(std::filesystem::path path);
  [[nodiscard]] static Location from_archive(const std::filesystem::path& archive_file,
                                             const std::filesystem::path& entry = {});
  /// Virtual listing of files with the given tag name (may include namespace / commas).
  [[nodiscard]] static Location from_tag(std::string_view tag_name);
  [[nodiscard]] static Location from_url(std::string_view url);
  [[nodiscard]] static Location from_human(std::string_view text);

  [[nodiscard]] std::string as_url() const;
  /// For file locations: the filesystem path.
  /// For archive locations: the archive file path (not the extracted tree).
  /// For tag locations: empty (use tag_query()).
  [[nodiscard]] std::filesystem::path as_path() const;

  [[nodiscard]] bool is_archive() const noexcept { return protocol_ == "archive"; }
  [[nodiscard]] bool is_file() const noexcept { return protocol_ == "file"; }
  [[nodiscard]] bool is_tag() const noexcept { return protocol_ == "tag"; }

  /// Path inside the archive (empty = archive root). Only meaningful if is_archive().
  [[nodiscard]] std::filesystem::path entry_path() const { return entry_; }

  /// Tag query string (e.g. "work" or "foo,bar"). Only meaningful if is_tag().
  [[nodiscard]] std::string tag_query() const;

  [[nodiscard]] Location parent() const;
  [[nodiscard]] Location join(std::string_view child) const;

  [[nodiscard]] std::string basename() const;
  [[nodiscard]] std::string dirname() const;

  [[nodiscard]] bool empty() const noexcept { return path_.empty(); }

  [[nodiscard]] bool operator==(const Location&) const = default;
  [[nodiscard]] auto operator<=>(const Location&) const = default;

private:
  Location(std::string protocol, std::filesystem::path path, std::filesystem::path entry);

  std::string protocol_{"file"};
  std::filesystem::path path_;
  std::filesystem::path entry_;
};

/// Heuristic: common archive extensions (zip, tar, 7z, rar, …).
[[nodiscard]] bool looks_like_archive(const std::filesystem::path& path);

} // namespace dirtoo::fs

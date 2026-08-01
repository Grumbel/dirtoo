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
///
/// URL encoding for archives (Python-style, preferred):
///   file:///abs/path/to.zip//archive
///   file:///abs/path/to.zip//archive:inner/dir
/// Legacy JAR-inspired form still accepted by from_url:
///   archive:///abs/path/to.zip!/inner/dir
class Location {
public:
  Location() = default;

  [[nodiscard]] static Location from_path(const std::filesystem::path& path);
  /// Like from_path but skips weakly_canonical — use for directory children when the
  /// parent is already absolute/normalized (listing hot path).
  [[nodiscard]] static Location from_path_unchecked(std::filesystem::path path);
  [[nodiscard]] static Location from_archive(const std::filesystem::path& archive_file,
                                             const std::filesystem::path& entry = {});
  [[nodiscard]] static Location from_url(std::string_view url);
  [[nodiscard]] static Location from_human(std::string_view text);

  [[nodiscard]] std::string as_url() const;
  /// For file locations: the filesystem path.
  /// For archive locations: the archive file path (not the extracted tree).
  [[nodiscard]] std::filesystem::path as_path() const;

  [[nodiscard]] bool is_archive() const noexcept { return protocol_ == "archive"; }
  [[nodiscard]] bool is_file() const noexcept { return protocol_ == "file"; }

  /// Path inside the archive (empty = archive root). Only meaningful if is_archive().
  [[nodiscard]] std::filesystem::path entry_path() const { return entry_; }

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

/// Heuristic: common archive extensions.
[[nodiscard]] bool looks_like_archive(const std::filesystem::path& path);

} // namespace dirtoo::fs

namespace std {
template <>
struct hash<dirtoo::fs::Location> {
  size_t operator()(const dirtoo::fs::Location& loc) const noexcept
  {
    return hash<string>{}(loc.as_url());
  }
};
} // namespace std

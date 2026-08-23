// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/filter/media_probe.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace dirtoo::filter {

/// Fingerprint used to invalidate cached media rows when the file changes.
struct FileFingerprint {
  std::int64_t mtime_ns = 0;
  std::uint64_t size = 0;
  bool valid = false;
};

[[nodiscard]] FileFingerprint fingerprint_file(const std::filesystem::path& path);

/// Process-wide media metadata cache: memory + SQLite, probed on worker threads.
/// GUI code must only call try_get() / request(); never probe_media() on the UI thread.
class MediaMetaCache {
public:
  using ReadyCallback = std::function<void(const std::string& path, std::optional<MediaInfo> info,
                                           std::uint64_t generation)>;

  static MediaMetaCache& instance();

  /// Open SQLite DB (idempotent). Empty path → default XDG cache location.
  void open(const std::filesystem::path& db_path = {});

  /// Close DB and stop workers (tests / shutdown).
  void close();

  /// In-memory lookup only — no disk I/O. nullopt means unknown / not yet loaded.
  [[nodiscard]] std::optional<MediaInfo> try_get(const std::filesystem::path& path) const;

  /// True if we already know this path has no usable media metadata.
  [[nodiscard]] bool is_negative(const std::filesystem::path& path) const;

  /// Enqueue background resolve (SQLite → ffprobe if needed). Callback may run on a
  /// worker thread; callers that touch Qt must queue to the GUI thread.
  void request(const std::filesystem::path& path, std::uint64_t generation = 0,
               ReadyCallback on_ready = {});

  /// Bump generation so in-flight work can be ignored by callers.
  std::uint64_t bump_generation();

  [[nodiscard]] std::uint64_t generation() const;

  /// Default DB path: $XDG_CACHE_HOME/dirtoo/meta.sqlite
  [[nodiscard]] static std::filesystem::path default_db_path();

  MediaMetaCache(const MediaMetaCache&) = delete;
  MediaMetaCache& operator=(const MediaMetaCache&) = delete;

private:
  MediaMetaCache();
  ~MediaMetaCache();

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/// Synchronous resolve for CLI tools (may hit SQLite + ffprobe). Do not call from GUI.
[[nodiscard]] std::optional<MediaInfo> resolve_media_cached(const std::filesystem::path& path);

} // namespace dirtoo::filter

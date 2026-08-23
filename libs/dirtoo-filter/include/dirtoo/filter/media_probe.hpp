// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace dirtoo::filter {

/// Optional media properties obtained via ffprobe (or empty if unavailable).
struct MediaInfo {
  std::optional<std::uint32_t> width;
  std::optional<std::uint32_t> height;
  /// Duration in milliseconds (matches Python MetaData / mediainfo).
  std::optional<std::uint64_t> duration_ms;
  std::optional<double> framerate;
  std::optional<std::uint64_t> pages;
  std::optional<std::uint64_t> file_count;
};

/// Probe path with ffprobe (process memory cache only). Uses $DIRTOO_FFPROBE or
/// `ffprobe` on PATH. Prefer MediaMetaCache for GUI code (no I/O on UI thread).
/// Returns nullopt if the tool is missing or the file has no usable streams.
[[nodiscard]] std::optional<MediaInfo> probe_media(const std::filesystem::path& path);

/// ffprobe only — no caches. Used by MediaMetaCache workers.
[[nodiscard]] std::optional<MediaInfo> probe_media_raw(const std::filesystem::path& path);

/// Parse human duration to seconds: plain number, `1h2m3s`, `mm:ss`, `h:mm:ss`.
[[nodiscard]] std::optional<double> parse_duration_seconds(std::string_view text);

/// Clear the process-wide media probe cache (tests).
void clear_media_probe_cache();

} // namespace dirtoo::filter

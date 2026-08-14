// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <filesystem>

namespace dirtoo::app {

/// Shared MIME helpers. Thumbnailer1 is driven by the MIME *we* pass; wrong
/// type → wrong/no generator. Prefer extension for bulk/fast paths; use content
/// when correctness matters or after a generator failure.

/// Extension only (no disk read). Suitable for large listings / first thumb guess.
[[nodiscard]] QString mime_from_extension(const QString& path);
[[nodiscard]] QString mime_from_extension(const std::filesystem::path& path);

/// Content magic (reads file). Use off the GUI thread or for single-file paths.
[[nodiscard]] QString mime_from_content(const QString& path);
[[nodiscard]] QString mime_from_content(const std::filesystem::path& path);

/// Qt MatchDefault: extension first, then content for local files.
[[nodiscard]] QString mime_from_default(const QString& path);
[[nodiscard]] QString mime_from_default(const std::filesystem::path& path);

/// Fast first guess for Thumbnailer1: extension of the full path; never empty.
[[nodiscard]] QString mime_for_thumbnail_fast(const QString& path);
[[nodiscard]] QString mime_for_thumbnail_fast(const std::filesystem::path& path);

/// True when @p a and @p b are the same type for thumbnail purposes
/// (exact match, or both under image/* / video/* with same subtype).
[[nodiscard]] bool mime_equivalent_for_thumb(const QString& a, const QString& b);

/// Types that normally produce a thumbnail (badge on hard failure).
[[nodiscard]] bool mime_expects_thumbnail(const QString& mime);

} // namespace dirtoo::app

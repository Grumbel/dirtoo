// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <filesystem>
#include <vector>

class QMimeData;

namespace dirtoo::app {

enum class ClipboardMode {
  Copy,
  Cut,
};

struct ClipboardPayload {
  ClipboardMode mode = ClipboardMode::Copy;
  std::vector<std::filesystem::path> paths;
};

/// Build MIME data for internal + text/uri-list + GNOME interop.
[[nodiscard]] QMimeData* make_clipboard_mime(ClipboardMode mode,
                                             const std::vector<std::filesystem::path>& paths);

/// Parse clipboard contents.
[[nodiscard]] ClipboardPayload parse_clipboard_mime(const QMimeData* mime);

[[nodiscard]] bool clipboard_has_paths(const QMimeData* mime);

/// Testable parsers for textual clipboard payloads.
[[nodiscard]] ClipboardPayload parse_dirtoo_clipboard_text(const QString& text);
[[nodiscard]] ClipboardPayload parse_gnome_clipboard_text(const QString& text);

} // namespace dirtoo::app

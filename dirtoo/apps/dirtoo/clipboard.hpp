// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

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

/// Build MIME data for internal + text/uri-list interop.
[[nodiscard]] QMimeData* make_clipboard_mime(ClipboardMode mode,
                                             const std::vector<std::filesystem::path>& paths);

/// Parse clipboard contents. Supports application/x-dirtoo-clipboard and text/uri-list.
[[nodiscard]] ClipboardPayload parse_clipboard_mime(const QMimeData* mime);

[[nodiscard]] bool clipboard_has_paths(const QMimeData* mime);

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "clipboard.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/fs/location.hpp"

#include <QString>

#include <filesystem>
#include <vector>

namespace dirtoo::app {

/// Status-bar verb for a successful clipboard mark (copy/cut/link).
[[nodiscard]] QString clipboard_mode_verb(ClipboardMode mode);

/// Collect filesystem paths from a selection (preserves order).
[[nodiscard]] std::vector<std::filesystem::path>
paths_from_fileinfos(const std::vector<fs::FileInfo>& files);

/// True when the current location can host create/paste/delete (not archive/tag).
[[nodiscard]] bool location_allows_filesystem_mutations(const fs::Location& location);

/// Put paths on the system clipboard (dirtoo + GNOME + uri-list MIME).
void apply_paths_to_system_clipboard(ClipboardMode mode,
                                     const std::vector<std::filesystem::path>& paths);

} // namespace dirtoo::app

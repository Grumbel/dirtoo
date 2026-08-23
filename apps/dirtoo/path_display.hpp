// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <QString>

namespace dirtoo::app {

/// Collapse intermediate path segments when the path is long.
/// Example: /usr/local/bin/tool -> /u…/l…/b…/tool (basename always full).
[[nodiscard]] QString elide_path_for_title(QString path, int max_chars = 96);

/// Human path or archive/tag URL for titles and chrome.
[[nodiscard]] QString location_display_path(const fs::Location& loc);

struct WindowTitleTexts {
  QString window_title; ///< Full title bar text
  QString icon_text;    ///< Compact taskbar / icon name
};

/// Build window title and icon text for the current location.
[[nodiscard]] WindowTitleTexts make_window_title_texts(const fs::Location& location,
                                                        bool read_only);

} // namespace dirtoo::app

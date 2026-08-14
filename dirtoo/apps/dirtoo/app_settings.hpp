// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace dirtoo::app {

struct AppSettings {
  QString view_mode = QStringLiteral("detail");
  /// Per-view zoom indices (Icons / List / Detail).
  int zoom_icons = 2;
  int zoom_list = 2;
  int zoom_detail = 2;
  /// @deprecated migrated into zoom_* ; kept for one-shot load from old configs.
  int zoom_index = 2;
  int icon_detail_level = 3;
  bool crop_thumbnails = false;
  /// Gap between icon tiles (pixels). Icons / RelativeIcons views.
  int icon_spacing = 12;
  /// Extra width past the thumbnail for the caption (pixels).
  int icon_cell_padding = 0;  // extra tile width beyond zoom size (0 = dense)
  /// Detail-view columns that are visible (logical names). Name is always on.
  /// Defaults include Dimensions+Framerate+Duration; Width/Height optional.
  QStringList detail_columns{QStringLiteral("size"), QStringLiteral("dimensions"),
                             QStringLiteral("aspectratio"), QStringLiteral("framerate"),
                             QStringLiteral("duration"), QStringLiteral("modified"),
                             QStringLiteral("type")};
  bool show_hidden = false;
  /// Highlight files not yet opened (OpenedFilesStore) — unread-mail style.
  bool show_opened_state = false;
  /// CSS-like #RRGGBB or #AARRGGBB for the unopened highlight fill/edge base.
  QString unopened_highlight_color = QStringLiteral("#3B82F6");
  bool show_filter = false; ///< Filter bar hidden until shown (View menu / pin).
  bool filter_pinned = false;
  bool show_sidebar = true;
  int sidebar_width = 220;
  /// When true, filesystem mutations (delete, rename, paste, mkdir, …) are blocked.
  bool read_only = false;
  bool directories_first = true;
  /// Default / restored sort key (name|size|extension|modified|type|…).
  QString sort_key = QStringLiteral("name");
  /// When true, sort ascending (false = reverse order).
  bool sort_ascending = true;
  QString group_mode = QStringLiteral("none"); // none|day|directory|duration|session
  /// Size display units: "si" (kB/MB, base 1000) or "iec" (KiB/MiB, base 1024).
  QString size_units = QStringLiteral("si");
  /// Preferred desktop application id for double-click open (e.g. "org.gnome.gedit.desktop").
  /// Empty = system default via xdg / QDesktopServices.
  QString default_open_desktop_id;
  /// How to hash large files in Checksum dialog / before tagging:
  /// "full" always full SHA-256; "quick" prefer sample hash when over threshold;
  /// "prompt" ask Full vs Quick when any selected file exceeds threshold.
  QString hash_policy = QStringLiteral("prompt");
  /// Size threshold in MiB for hash_policy prompt/quick (regular files only).
  int hash_large_mib = 64;
  QByteArray window_geometry;
  QByteArray window_state;
  QString last_location;
  /// Persistent location history (URLs / paths), most recent last.
  QStringList location_history;
  /// When true, skip the under-development startup warning dialog.
  bool dismiss_dev_warning = false;
};

[[nodiscard]] AppSettings load_settings();
void save_settings(const AppSettings& settings);

} // namespace dirtoo::app

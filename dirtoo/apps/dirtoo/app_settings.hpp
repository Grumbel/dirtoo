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
  /// Detail-view columns that are visible (logical names). Name is always on.
  /// Defaults include Dimensions+Framerate+Duration; Width/Height optional.
  QStringList detail_columns{QStringLiteral("size"), QStringLiteral("dimensions"),
                             QStringLiteral("aspectratio"), QStringLiteral("framerate"),
                             QStringLiteral("duration"), QStringLiteral("modified"),
                             QStringLiteral("type")};
  bool show_hidden = false;
  bool show_filter = true;
  bool filter_pinned = false;
  bool show_sidebar = true;
  int sidebar_width = 220;
  /// When true, filesystem mutations (delete, rename, paste, mkdir, …) are blocked.
  bool read_only = false;
  bool directories_first = true;
  QString group_mode = QStringLiteral("none"); // none|day|directory|duration|session
  /// Size display units: "si" (kB/MB, base 1000) or "iec" (KiB/MiB, base 1024).
  QString size_units = QStringLiteral("si");
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

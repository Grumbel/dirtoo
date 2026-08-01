// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace dirtoo::app {

struct AppSettings {
  QString view_mode = QStringLiteral("detail");
  int zoom_index = 2;
  int icon_detail_level = 3;
  bool crop_thumbnails = false;
  bool show_hidden = false;
  bool show_filter = true;
  bool filter_pinned = false;
  bool directories_first = true;
  QString group_mode = QStringLiteral("none"); // none|day|directory|duration
  /// Size display units: "si" (KB/MB, base 1000) or "iec" (KiB/MiB, base 1024).
  QString size_units = QStringLiteral("si");
  QByteArray window_geometry;
  QByteArray window_state;
  QString last_location;
  /// Persistent location history (URLs / paths), most recent last.
  QStringList location_history;
};

[[nodiscard]] AppSettings load_settings();
void save_settings(const AppSettings& settings);

} // namespace dirtoo::app

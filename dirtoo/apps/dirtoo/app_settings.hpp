// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

namespace dirtoo::app {

struct AppSettings {
  QString view_mode = QStringLiteral("detail");
  int zoom_index = 2;
  bool show_hidden = false;
  QByteArray window_geometry;
  QByteArray window_state;
  QString last_location;
};

[[nodiscard]] AppSettings load_settings();
void save_settings(const AppSettings& settings);

} // namespace dirtoo::app

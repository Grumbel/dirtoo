// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app_settings.hpp"

#include <QSettings>

namespace dirtoo::app {

AppSettings load_settings()
{
  QSettings s(QStringLiteral("dirtoo"), QStringLiteral("dirtoo"));
  AppSettings out;
  out.view_mode = s.value(QStringLiteral("ui/view_mode"), out.view_mode).toString();
  out.zoom_index = s.value(QStringLiteral("ui/zoom_index"), out.zoom_index).toInt();
  out.icon_detail_level = s.value(QStringLiteral("ui/icon_detail_level"), out.icon_detail_level).toInt();
  out.crop_thumbnails = s.value(QStringLiteral("ui/crop_thumbnails"), out.crop_thumbnails).toBool();
  out.show_hidden = s.value(QStringLiteral("ui/show_hidden"), out.show_hidden).toBool();
  out.show_filter = s.value(QStringLiteral("ui/show_filter"), out.show_filter).toBool();
  out.filter_pinned = s.value(QStringLiteral("ui/filter_pinned"), out.filter_pinned).toBool();
  out.window_geometry = s.value(QStringLiteral("window/geometry")).toByteArray();
  out.window_state = s.value(QStringLiteral("window/state")).toByteArray();
  out.last_location = s.value(QStringLiteral("session/last_location")).toString();
  return out;
}

void save_settings(const AppSettings& settings)
{
  QSettings s(QStringLiteral("dirtoo"), QStringLiteral("dirtoo"));
  s.setValue(QStringLiteral("ui/view_mode"), settings.view_mode);
  s.setValue(QStringLiteral("ui/zoom_index"), settings.zoom_index);
  s.setValue(QStringLiteral("ui/icon_detail_level"), settings.icon_detail_level);
  s.setValue(QStringLiteral("ui/crop_thumbnails"), settings.crop_thumbnails);
  s.setValue(QStringLiteral("ui/show_hidden"), settings.show_hidden);
  s.setValue(QStringLiteral("ui/show_filter"), settings.show_filter);
  s.setValue(QStringLiteral("ui/filter_pinned"), settings.filter_pinned);
  s.setValue(QStringLiteral("window/geometry"), settings.window_geometry);
  s.setValue(QStringLiteral("window/state"), settings.window_state);
  s.setValue(QStringLiteral("session/last_location"), settings.last_location);
}

} // namespace dirtoo::app

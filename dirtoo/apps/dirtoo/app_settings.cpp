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
  out.zoom_icons = s.value(QStringLiteral("ui/zoom_icons"), out.zoom_index).toInt();
  out.zoom_list = s.value(QStringLiteral("ui/zoom_list"), out.zoom_index).toInt();
  out.zoom_detail = s.value(QStringLiteral("ui/zoom_detail"), out.zoom_index).toInt();
  if (s.contains(QStringLiteral("ui/detail_columns"))) {
    out.detail_columns = s.value(QStringLiteral("ui/detail_columns")).toStringList();
  }
  out.icon_detail_level = s.value(QStringLiteral("ui/icon_detail_level"), out.icon_detail_level).toInt();
  out.crop_thumbnails = s.value(QStringLiteral("ui/crop_thumbnails"), out.crop_thumbnails).toBool();
  out.show_hidden = s.value(QStringLiteral("ui/show_hidden"), out.show_hidden).toBool();
  out.show_filter = s.value(QStringLiteral("ui/show_filter"), out.show_filter).toBool();
  out.filter_pinned = s.value(QStringLiteral("ui/filter_pinned"), out.filter_pinned).toBool();
  out.show_sidebar = s.value(QStringLiteral("ui/show_sidebar"), out.show_sidebar).toBool();
  out.sidebar_width = s.value(QStringLiteral("ui/sidebar_width"), out.sidebar_width).toInt();
  out.read_only = s.value(QStringLiteral("ui/read_only"), out.read_only).toBool();
  out.directories_first = s.value(QStringLiteral("ui/directories_first"), out.directories_first).toBool();
  out.group_mode = s.value(QStringLiteral("ui/group_mode"), out.group_mode).toString();
  out.size_units = s.value(QStringLiteral("ui/size_units"), out.size_units).toString();
  out.window_geometry = s.value(QStringLiteral("window/geometry")).toByteArray();
  out.window_state = s.value(QStringLiteral("window/state")).toByteArray();
  out.last_location = s.value(QStringLiteral("session/last_location")).toString();
  out.location_history = s.value(QStringLiteral("session/location_history")).toStringList();
  out.dismiss_dev_warning =
      s.value(QStringLiteral("ui/dismiss_dev_warning"), out.dismiss_dev_warning).toBool();
  return out;
}

void save_settings(const AppSettings& settings)
{
  QSettings s(QStringLiteral("dirtoo"), QStringLiteral("dirtoo"));
  s.setValue(QStringLiteral("ui/view_mode"), settings.view_mode);
  s.setValue(QStringLiteral("ui/zoom_index"), settings.zoom_icons); // legacy single key
  s.setValue(QStringLiteral("ui/zoom_icons"), settings.zoom_icons);
  s.setValue(QStringLiteral("ui/zoom_list"), settings.zoom_list);
  s.setValue(QStringLiteral("ui/zoom_detail"), settings.zoom_detail);
  s.setValue(QStringLiteral("ui/detail_columns"), settings.detail_columns);
  s.setValue(QStringLiteral("ui/icon_detail_level"), settings.icon_detail_level);
  s.setValue(QStringLiteral("ui/crop_thumbnails"), settings.crop_thumbnails);
  s.setValue(QStringLiteral("ui/show_hidden"), settings.show_hidden);
  s.setValue(QStringLiteral("ui/show_filter"), settings.show_filter);
  s.setValue(QStringLiteral("ui/filter_pinned"), settings.filter_pinned);
  s.setValue(QStringLiteral("ui/show_sidebar"), settings.show_sidebar);
  s.setValue(QStringLiteral("ui/sidebar_width"), settings.sidebar_width);
  s.setValue(QStringLiteral("ui/read_only"), settings.read_only);
  s.setValue(QStringLiteral("ui/directories_first"), settings.directories_first);
  s.setValue(QStringLiteral("ui/group_mode"), settings.group_mode);
  s.setValue(QStringLiteral("ui/size_units"), settings.size_units);
  s.setValue(QStringLiteral("window/geometry"), settings.window_geometry);
  s.setValue(QStringLiteral("window/state"), settings.window_state);
  s.setValue(QStringLiteral("session/last_location"), settings.last_location);
  s.setValue(QStringLiteral("session/location_history"), settings.location_history);
  s.setValue(QStringLiteral("ui/dismiss_dev_warning"), settings.dismiss_dev_warning);
}

} // namespace dirtoo::app

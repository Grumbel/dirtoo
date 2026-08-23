// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sort_settings.hpp"

namespace dirtoo::app {

collection::SortKey sort_key_from_settings_string(const QString& s)
{
  const QString k = s.toLower();
  if (k == QLatin1String("size")) {
    return collection::SortKey::Size;
  }
  if (k == QLatin1String("extension")) {
    return collection::SortKey::Extension;
  }
  if (k == QLatin1String("modified") || k == QLatin1String("date")) {
    return collection::SortKey::Modified;
  }
  if (k == QLatin1String("type")) {
    return collection::SortKey::Type;
  }
  if (k == QLatin1String("width")) {
    return collection::SortKey::Width;
  }
  if (k == QLatin1String("height")) {
    return collection::SortKey::Height;
  }
  if (k == QLatin1String("resolution") || k == QLatin1String("dimensions")) {
    return collection::SortKey::Resolution;
  }
  if (k == QLatin1String("aspectratio") || k == QLatin1String("aspect")) {
    return collection::SortKey::AspectRatio;
  }
  if (k == QLatin1String("duration")) {
    return collection::SortKey::Duration;
  }
  if (k == QLatin1String("framerate") || k == QLatin1String("fps")) {
    return collection::SortKey::Framerate;
  }
  if (k == QLatin1String("permissions")) {
    return collection::SortKey::Permissions;
  }
  if (k == QLatin1String("random")) {
    return collection::SortKey::Random;
  }
  return collection::SortKey::Name;
}

QString sort_key_to_settings_string(collection::SortKey key)
{
  switch (key) {
  case collection::SortKey::Size:
    return QStringLiteral("size");
  case collection::SortKey::Extension:
    return QStringLiteral("extension");
  case collection::SortKey::Modified:
    return QStringLiteral("modified");
  case collection::SortKey::Type:
    return QStringLiteral("type");
  case collection::SortKey::Width:
    return QStringLiteral("width");
  case collection::SortKey::Height:
    return QStringLiteral("height");
  case collection::SortKey::Resolution:
    return QStringLiteral("resolution");
  case collection::SortKey::AspectRatio:
    return QStringLiteral("aspectratio");
  case collection::SortKey::Duration:
    return QStringLiteral("duration");
  case collection::SortKey::Framerate:
    return QStringLiteral("framerate");
  case collection::SortKey::Permissions:
    return QStringLiteral("permissions");
  case collection::SortKey::Random:
    return QStringLiteral("random");
  case collection::SortKey::Name:
  default:
    return QStringLiteral("name");
  }
}

} // namespace dirtoo::app

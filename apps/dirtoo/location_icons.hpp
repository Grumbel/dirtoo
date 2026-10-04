// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"
#include "dirtoo/thumbnail/thumbnailer.hpp"

#include <QFileIconProvider>
#include <QFileInfo>
#include <QIcon>
#include <QString>

namespace dirtoo::app {

/// Icon for a location in History / Bookmarks menus: cached thumbnail when
/// present, otherwise the folder (or archive package) icon.
///
/// Never stats the location itself — history entries may sit on slow,
/// unplugged or hung drives, and menus must open instantly. Only the local
/// XDG thumbnail cache is probed; availability of the location is checked in
/// the background by PathAvailability.
inline QIcon icon_for_location(const fs::Location& loc)
{
  if (loc.is_archive()) {
    const QIcon pkg = QIcon::fromTheme(QStringLiteral("package-x-generic"));
    if (!pkg.isNull()) {
      return pkg;
    }
    return QIcon::fromTheme(QStringLiteral("folder"));
  }

  if (loc.is_file()) {
    const QString large = thumbnail::Thumbnailer::cache_path_for(loc, QStringLiteral("large"));
    if (QFileInfo::exists(large)) {
      return QIcon(large);
    }
    const QString normal = thumbnail::Thumbnailer::cache_path_for(loc, QStringLiteral("normal"));
    if (QFileInfo::exists(normal)) {
      return QIcon(normal);
    }
  }

  // Navigated locations are directories (tag:// / set:// views included).
  static const QIcon folder = QFileIconProvider().icon(QFileIconProvider::Folder);
  return folder;
}

} // namespace dirtoo::app

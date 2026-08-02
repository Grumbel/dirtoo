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
/// present, otherwise the system folder/file icon (or archive package icon).
inline QIcon icon_for_location(const fs::Location& loc)
{
  if (loc.is_archive()) {
    const QIcon pkg = QIcon::fromTheme(QStringLiteral("package-x-generic"));
    if (!pkg.isNull()) {
      return pkg;
    }
    return QIcon::fromTheme(QStringLiteral("folder"));
  }

  const QString large =
      thumbnail::Thumbnailer::cache_path_for(loc, QStringLiteral("large"));
  if (QFileInfo::exists(large)) {
    return QIcon(large);
  }
  const QString normal =
      thumbnail::Thumbnailer::cache_path_for(loc, QStringLiteral("normal"));
  if (QFileInfo::exists(normal)) {
    return QIcon(normal);
  }

  QFileIconProvider provider;
  const QFileInfo fi(QString::fromStdString(loc.as_path().string()));
  if (fi.isDir() || !fi.exists()) {
    // Non-existent paths still show as folders in history (navigated dirs).
    return provider.icon(QFileIconProvider::Folder);
  }
  return provider.icon(fi);
}

} // namespace dirtoo::app

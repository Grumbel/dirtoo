// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QPixmap>
#include <QString>
#include <QStringList>

#ifndef DIRTOO_ICON_DIR
#  define DIRTOO_ICON_DIR ""
#endif

namespace dirtoo::app {

/// Directory containing badge / dnd / app icons (build tree or installed share).
inline QString icon_directory()
{
  static const QString cached = [] {
    QStringList candidates;
    const char* build_dir = DIRTOO_ICON_DIR;
    if (build_dir != nullptr && build_dir[0] != '\0') {
      candidates.append(QString::fromUtf8(build_dir));
    }
    candidates.append(QStringLiteral("/usr/share/dirtoo/icons"));
    candidates.append(QStringLiteral("/usr/local/share/dirtoo/icons"));
    const QString app = QCoreApplication::applicationDirPath();
    candidates.append(app + QStringLiteral("/../share/dirtoo/icons"));
    candidates.append(app + QStringLiteral("/share/dirtoo/icons"));
    candidates.append(app + QStringLiteral("/../../resources/icons"));
    candidates.append(app + QStringLiteral("/../resources/icons"));

    for (const QString& dir : candidates) {
      if (dir.isEmpty()) {
        continue;
      }
      if (QFile::exists(dir + QStringLiteral("/badge-image.png"))
          || QFile::exists(dir + QStringLiteral("/dirtoo.png"))
          || QFile::exists(dir + QStringLiteral("/view-icons.svg"))) {
        return QDir(dir).absolutePath();
      }
    }
    qWarning().noquote() << QStringLiteral(
        "dirtoo: icon directory not found (tried build DIRTOO_ICON_DIR, "
        "/usr[/local]/share/dirtoo/icons, and paths relative to the executable); "
        "bundled toolbar/badge assets will be missing");
    return QString{};
  }();
  return cached;
}

inline QPixmap load_badge_pixmap(const QString& file_name)
{
  const QString dir = icon_directory();
  if (dir.isEmpty()) {
    // icon_directory() already logged once.
    return {};
  }
  const QString path = dir + QLatin1Char('/') + file_name;
  if (!QFile::exists(path)) {
    qWarning().noquote() << QStringLiteral("dirtoo: icon asset not found:") << path;
    return {};
  }
  QPixmap pm(path);
  if (pm.isNull()) {
    qWarning().noquote() << QStringLiteral("dirtoo: failed to load icon asset:") << path;
  }
  return pm;
}

} // namespace dirtoo::app

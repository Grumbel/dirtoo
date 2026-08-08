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
        "falling back to embedded :/icons/ resources where available");
    return QString{};
  }();
  return cached;
}

/// Load a badge / toolbar icon by file name (e.g. "badge-loading.png").
/// Tries the on-disk icon directory first, then the embedded Qt resource
/// ":/icons/<file_name>". Failures are always reported with qWarning (stderr).
inline QPixmap load_badge_pixmap(const QString& file_name)
{
  if (file_name.isEmpty()) {
    qWarning().noquote() << QStringLiteral("dirtoo: load_badge_pixmap called with empty name");
    return {};
  }

  const QString dir = icon_directory();
  if (!dir.isEmpty()) {
    const QString path = dir + QLatin1Char('/') + file_name;
    if (QFile::exists(path)) {
      QPixmap pm(path);
      if (!pm.isNull()) {
        return pm;
      }
      qWarning().noquote() << QStringLiteral(
          "dirtoo: failed to decode icon asset (corrupt or unsupported):")
                           << path;
    } else {
      qWarning().noquote() << QStringLiteral("dirtoo: icon asset not found on disk:") << path
                           << QStringLiteral("(will try embedded resource)");
    }
  }

  // Embedded qrc fallback (resources.qrc aliases under :/icons/).
  const QString resource = QStringLiteral(":/icons/") + file_name;
  if (QFile::exists(resource)) {
    QPixmap pm(resource);
    if (!pm.isNull()) {
      return pm;
    }
    qWarning().noquote() << QStringLiteral(
        "dirtoo: failed to decode embedded icon resource:")
                         << resource;
    return {};
  }

  qWarning().noquote() << QStringLiteral(
      "dirtoo: icon asset missing from disk and embedded resources:")
                       << file_name;
  return {};
}

} // namespace dirtoo::app

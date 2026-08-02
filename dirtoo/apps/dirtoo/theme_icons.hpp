// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "badge_icons.hpp"

#include <QFile>
#include <QHash>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QSvgRenderer>
#include <QtDebug>

namespace dirtoo::app {

/// Prefer bundled SVG/PNG under resources/icons, then FreeDesktop theme icons.
inline QIcon theme_icon(const char* name, const char* fallback = nullptr)
{
  static const QHash<QString, QString> kBundled{
      {QStringLiteral("view-grid"), QStringLiteral("view-icons.svg")},
      {QStringLiteral("view-list-icons"), QStringLiteral("view-icons.svg")},
      {QStringLiteral("view-list"), QStringLiteral("view-small-icons.svg")},
      {QStringLiteral("view-list-details"), QStringLiteral("view-detail.svg")},
      {QStringLiteral("view-hidden"), QStringLiteral("view-hidden.svg")},
      {QStringLiteral("view-filter"), QStringLiteral("view-hidden.svg")},
      {QStringLiteral("view-sidetree"), QStringLiteral("view-sidebar.svg")},
      {QStringLiteral("view-list-tree"), QStringLiteral("view-sidebar.svg")},
      {QStringLiteral("zoom-in"), QStringLiteral("zoom-in.svg")},
      {QStringLiteral("zoom-out"), QStringLiteral("zoom-out.svg")},
      {QStringLiteral("zoom-fit-best"), QStringLiteral("icon-detail-more.svg")},
      {QStringLiteral("list-add"), QStringLiteral("icon-detail-more.svg")},
      {QStringLiteral("zoom-original"), QStringLiteral("icon-detail-less.svg")},
      {QStringLiteral("list-remove"), QStringLiteral("icon-detail-less.svg")},
      {QStringLiteral("crop-thumbnails"), QStringLiteral("crop-thumbnails.svg")},
  };

  // SVGs need Qt Svg (QSvgRenderer): QIcon(path) alone fails without the svg image plugin.
  const auto load_bundled = [](const QString& file, bool required) -> QIcon {
    const QString dir = icon_directory();
    if (dir.isEmpty() || file.isEmpty()) {
      return {};
    }
    const QString path = dir + QLatin1Char('/') + file;
    if (!QFile::exists(path)) {
      if (required) {
        static QStringList logged;
        if (!logged.contains(path)) {
          logged.append(path);
          qWarning().noquote() << QStringLiteral("dirtoo: bundled icon asset not found:") << path;
        }
      }
      return {};
    }

    QIcon icon;
    if (path.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) {
      QSvgRenderer renderer(path);
      if (!renderer.isValid()) {
        if (required) {
          static QStringList logged_svg;
          if (!logged_svg.contains(path)) {
            logged_svg.append(path);
            qWarning().noquote() << QStringLiteral("dirtoo: invalid SVG icon:") << path;
          }
        }
        return {};
      }
      for (int s : {16, 22, 24, 32, 48, 64}) {
        QPixmap pm(s, s);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);
        painter.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&painter);
        painter.end();
        icon.addPixmap(pm);
      }
    } else {
      icon = QIcon(path);
    }

    if (icon.isNull() && required) {
      static QStringList logged_null;
      if (!logged_null.contains(path)) {
        logged_null.append(path);
        qWarning().noquote() << QStringLiteral("dirtoo: failed to load bundled icon:") << path;
      }
    }
    return icon;
  };

  const QString key = QString::fromUtf8(name);
  if (const auto it = kBundled.constFind(key); it != kBundled.cend()) {
    if (QIcon icon = load_bundled(*it, /*required=*/true); !icon.isNull()) {
      return icon;
    }
  }
  if (QIcon icon = load_bundled(key + QStringLiteral(".svg"), /*required=*/false); !icon.isNull()) {
    return icon;
  }
  if (QIcon icon = load_bundled(key + QStringLiteral(".png"), /*required=*/false); !icon.isNull()) {
    return icon;
  }

  QIcon icon = QIcon::fromTheme(key);
  if (icon.isNull() && fallback != nullptr) {
    const QString fb = QString::fromUtf8(fallback);
    if (const auto it = kBundled.constFind(fb); it != kBundled.cend()) {
      if (QIcon b = load_bundled(*it, /*required=*/true); !b.isNull()) {
        return b;
      }
    }
    icon = QIcon::fromTheme(fb);
  }
  return icon;
}

} // namespace dirtoo::app

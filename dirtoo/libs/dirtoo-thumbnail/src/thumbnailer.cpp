// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/thumbnail/thumbnailer.hpp"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QStandardPaths>

namespace dirtoo::thumbnail {

Thumbnailer::Thumbnailer(QObject* parent)
    : QObject(parent)
{
}

QString Thumbnailer::cache_path_for(const fs::Location& location, const QString& flavor)
{
  const QByteArray url = QByteArray::fromStdString(location.as_url());
  const QByteArray digest = QCryptographicHash::hash(url, QCryptographicHash::Md5).toHex();
  const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                       + QStringLiteral("/thumbnails/") + flavor;
  return base + QLatin1Char('/') + QString::fromLatin1(digest) + QStringLiteral(".png");
}

void Thumbnailer::request(const fs::Location& location, const QString& flavor)
{
  // D-Bus queueing will be implemented in Phase 6.
  const QString path = cache_path_for(location, flavor);
  if (QFileInfo::exists(path)) {
    emit thumbnail_ready(location, path);
  } else {
    emit thumbnail_failed(location, QStringLiteral("thumbnail not cached; D-Bus client not wired yet"));
  }
}

} // namespace dirtoo::thumbnail

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mime_util.hpp"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

namespace dirtoo::app {
namespace {

QMimeDatabase& mime_db()
{
  static QMimeDatabase db;
  return db;
}

QString name_or_octet(const QMimeType& mt)
{
  if (mt.isValid() && !mt.name().isEmpty()) {
    return mt.name();
  }
  return QStringLiteral("application/octet-stream");
}

} // namespace

QString mime_from_extension(const QString& path)
{
  if (path.isEmpty()) {
    return QStringLiteral("application/octet-stream");
  }
  // Directories have no useful extension; without this Open With never sees
  // apps that declare MimeType=inode/directory (or mimeapps.list entries).
  const QFileInfo fi(path);
  if (fi.isDir()) {
    return QStringLiteral("inode/directory");
  }
  // Full path (or at least a name with extension) — not basename-only.
  return name_or_octet(mime_db().mimeTypeForFile(path, QMimeDatabase::MatchExtension));
}

QString mime_from_extension(const std::filesystem::path& path)
{
  return mime_from_extension(QString::fromStdString(path.string()));
}

QString mime_from_content(const QString& path)
{
  if (path.isEmpty()) {
    return QStringLiteral("application/octet-stream");
  }
  const QFileInfo fi(path);
  if (fi.isDir()) {
    return QStringLiteral("inode/directory");
  }
  if (!fi.isFile()) {
    return mime_from_extension(path);
  }
  return name_or_octet(mime_db().mimeTypeForFile(fi, QMimeDatabase::MatchContent));
}

QString mime_from_content(const std::filesystem::path& path)
{
  return mime_from_content(QString::fromStdString(path.string()));
}

QString mime_from_default(const QString& path)
{
  if (path.isEmpty()) {
    return QStringLiteral("application/octet-stream");
  }
  const QFileInfo fi(path);
  if (fi.isDir()) {
    // MatchDefault on a directory is inode/directory; do not fall through to
    // extension-only matching (dirs have no extension → octet-stream).
    return QStringLiteral("inode/directory");
  }
  if (fi.isFile()) {
    return name_or_octet(mime_db().mimeTypeForFile(fi, QMimeDatabase::MatchDefault));
  }
  return mime_from_extension(path);
}

QString mime_from_default(const std::filesystem::path& path)
{
  return mime_from_default(QString::fromStdString(path.string()));
}

QString mime_for_thumbnail_fast(const QString& path)
{
  return mime_from_extension(path);
}

QString mime_for_thumbnail_fast(const std::filesystem::path& path)
{
  return mime_from_extension(path);
}

bool mime_equivalent_for_thumb(const QString& a, const QString& b)
{
  if (a.isEmpty() || b.isEmpty()) {
    return false;
  }
  if (a == b) {
    return true;
  }
  // Some DBs report image/jpg vs image/jpeg — treat as equivalent.
  auto norm = [](QString m) {
    if (m == QLatin1String("image/jpg")) {
      return QStringLiteral("image/jpeg");
    }
    return m;
  };
  return norm(a) == norm(b);
}

bool mime_expects_thumbnail(const QString& mime)
{
  if (mime.isEmpty()) {
    return false;
  }
  return mime.startsWith(QLatin1String("image/")) || mime.startsWith(QLatin1String("video/"))
         || mime == QLatin1String("application/pdf")
         || mime.contains(QLatin1String("opendocument"))
         || mime.contains(QLatin1String("officedocument"));
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mime_util.hpp"

#include <QByteArray>
#include <QFile>
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

// QMimeDatabase serialises every call on one global mutex, and the
// file-based entry points (mimeTypeForFile) open and read the file *while
// holding it*.  On a slow drive that stalls the GUI thread, whose cheap
// name-only lookups (mime_from_extension) then block on the mutex.  So read
// the bytes here, unlocked, and hand only the in-memory buffer to the
// database (mimeTypeForData / ...AndData never touch the filesystem).
QByteArray read_sniff_data(const QString& path)
{
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    return {};
  }
  return f.read(16384); // Qt's own sniff limit
}

} // namespace

QString mime_from_extension(const QString& path)
{
  if (path.isEmpty()) {
    return QStringLiteral("application/octet-stream");
  }
  // Glob match on the name only (mimeTypesForFileName never opens or stats).
  return name_or_octet(mime_db().mimeTypesForFileName(path).value(0));
}

QString mime_for_entry(const std::filesystem::path& path, bool is_directory)
{
  // Directories have no useful extension; without this Open With never sees
  // apps that declare MimeType=inode/directory (or mimeapps.list entries).
  if (is_directory) {
    return QStringLiteral("inode/directory");
  }
  return mime_from_extension(path);
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
  return name_or_octet(mime_db().mimeTypeForData(read_sniff_data(path)));
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
    // MatchDefault: an unambiguous glob wins without reading the file.
    const QList<QMimeType> globs = mime_db().mimeTypesForFileName(path);
    if (globs.size() == 1) {
      return name_or_octet(globs.first());
    }
    return name_or_octet(
        mime_db().mimeTypeForFileNameAndData(path, read_sniff_data(path)));
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

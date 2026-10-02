// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "file_icons.hpp"

#include <QFileIconProvider>
#include <QHash>
#include <QMimeDatabase>
#include <QMimeType>

namespace dirtoo::app {
namespace {

QFileIconProvider& icon_provider()
{
  // Only the type-based overload (Folder/File) is used — it does no I/O.
  static QFileIconProvider provider;
  return provider;
}

} // namespace

QIcon file_type_icon(const std::filesystem::path& path, bool is_directory)
{
  if (is_directory) {
    static const QIcon folder = icon_provider().icon(QFileIconProvider::Folder);
    return folder;
  }
  static QMimeDatabase db;
  static QHash<QString, QIcon> cache;
  // Name glob only; never opens or stats the file.
  const QMimeType mt =
      db.mimeTypesForFileName(QString::fromStdString(path.filename().string())).value(0);
  const QString key = mt.isValid() ? mt.name() : QString();
  if (const auto it = cache.constFind(key); it != cache.constEnd()) {
    return it.value();
  }
  const QIcon generic = icon_provider().icon(QFileIconProvider::File);
  QIcon icon = generic;
  if (mt.isValid()) {
    icon = QIcon::fromTheme(mt.iconName(), QIcon::fromTheme(mt.genericIconName(), generic));
  }
  cache.insert(key, icon);
  return icon;
}

QIcon file_type_icon(const fs::FileInfo& fi)
{
  return file_type_icon(fi.path(), fi.is_directory() || fi.symlink_target_is_directory());
}

} // namespace dirtoo::app

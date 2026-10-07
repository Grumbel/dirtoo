// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QLocale>
#include <QString>
#include <QStringList>

#include <optional>

namespace dirtoo::app {

/// The fields of a freedesktop "[Desktop Entry]" group that dirtoo uses.
struct DesktopEntry {
  QString name;        ///< Name, localised for the requested locale
  QString exec;
  QString icon;
  QString try_exec;
  QString path;        ///< working directory (Path=)
  QStringList mime_types;
  bool no_display = false;
  bool hidden = false;
  bool terminal = false;
};

/// Parse the "[Desktop Entry]" group of a .desktop file.
///
/// Not QSettings: its INI mode treats commas as list separators, so a
/// `Name=Foo, Bar` or an `Exec=env A=1,2 app` reads back as an empty string.
/// Values are unescaped per the spec (\s \n \t \r \\); `Name[de_DE]` /
/// `Name[de]` override the plain `Name` for a matching locale.
/// Returns nullopt when there is no [Desktop Entry] group.
[[nodiscard]] std::optional<DesktopEntry>
parse_desktop_entry(const QByteArray& data, const QLocale& locale = QLocale());

[[nodiscard]] std::optional<DesktopEntry>
parse_desktop_entry_file(const QString& path, const QLocale& locale = QLocale());

} // namespace dirtoo::app

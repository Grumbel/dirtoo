// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop_entry.hpp"

#include <QFile>

namespace dirtoo::app {
namespace {

QString unescape_value(const QString& raw)
{
  QString out;
  out.reserve(raw.size());
  for (int i = 0; i < raw.size(); ++i) {
    const QChar c = raw[i];
    if (c == QLatin1Char('\\') && i + 1 < raw.size()) {
      const QChar n = raw[i + 1];
      if (n == QLatin1Char('s')) {
        out += QLatin1Char(' ');
      } else if (n == QLatin1Char('n')) {
        out += QLatin1Char('\n');
      } else if (n == QLatin1Char('t')) {
        out += QLatin1Char('\t');
      } else if (n == QLatin1Char('r')) {
        out += QLatin1Char('\r');
      } else if (n == QLatin1Char('\\')) {
        out += QLatin1Char('\\');
      } else {
        out += c;  // unknown escape: keep as written
        continue;
      }
      ++i;
      continue;
    }
    out += c;
  }
  return out;
}

bool parse_bool(const QString& v)
{
  return v.trimmed().compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
}

} // namespace

std::optional<DesktopEntry> parse_desktop_entry(const QByteArray& data, const QLocale& locale)
{
  // Locale candidates for Name[...] in spec order: lang_COUNTRY, then lang.
  const QString full = locale.name();                      // e.g. "de_DE"
  const QString lang = full.section(QLatin1Char('_'), 0, 0);  // "de"

  bool in_entry = false;
  bool seen_entry = false;
  DesktopEntry e;
  QString name_default;
  QString name_lang;
  QString name_full;

  const QString text = QString::fromUtf8(data);
  const QStringList lines = text.split(QLatin1Char('\n'));
  for (const QString& raw : lines) {
    const QString line = raw.trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
      continue;
    }
    if (line.startsWith(QLatin1Char('['))) {
      in_entry = line == QLatin1String("[Desktop Entry]");
      seen_entry = seen_entry || in_entry;
      continue;
    }
    if (!in_entry) {
      continue;
    }
    const int eq = line.indexOf(QLatin1Char('='));
    if (eq <= 0) {
      continue;
    }
    const QString key = line.left(eq).trimmed();
    const QString value = unescape_value(line.mid(eq + 1).trimmed());

    if (key == QLatin1String("Name")) {
      name_default = value;
    } else if (key.startsWith(QLatin1String("Name["))) {
      const QString loc = key.mid(5, key.size() - 6);
      if (!full.isEmpty() && loc == full) {
        name_full = value;
      } else if (!lang.isEmpty() && loc == lang) {
        name_lang = value;
      }
    } else if (key == QLatin1String("Exec")) {
      e.exec = value;
    } else if (key == QLatin1String("TryExec")) {
      e.try_exec = value;
    } else if (key == QLatin1String("Icon")) {
      e.icon = value;
    } else if (key == QLatin1String("Path")) {
      e.path = value;
    } else if (key == QLatin1String("NoDisplay")) {
      e.no_display = parse_bool(value);
    } else if (key == QLatin1String("Hidden")) {
      e.hidden = parse_bool(value);
    } else if (key == QLatin1String("Terminal")) {
      e.terminal = parse_bool(value);
    } else if (key == QLatin1String("MimeType")) {
      e.mime_types = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
      for (QString& m : e.mime_types) {
        m = m.trimmed();
      }
    }
  }
  if (!seen_entry) {
    return std::nullopt;
  }
  e.name = !name_full.isEmpty() ? name_full : (!name_lang.isEmpty() ? name_lang : name_default);
  return e;
}

std::optional<DesktopEntry> parse_desktop_entry_file(const QString& path, const QLocale& locale)
{
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  return parse_desktop_entry(f.readAll(), locale);
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Query XDG .desktop files (Python dt-desktop).

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace {

struct DesktopEntry {
  QString path;
  QString id;
  QString name;
  QString exec;
  QString icon;
  QString try_exec;
  QStringList mime_types;
  bool no_display = false;
};

QStringList application_dirs()
{
  QStringList dirs;
  for (const QString& root : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)) {
    dirs << root;
  }
  // Extra common paths not always in ApplicationsLocation.
  const QString home = QDir::homePath();
  for (const QString& extra :
       {home + QStringLiteral("/.local/share/applications"),
        QStringLiteral("/usr/share/applications"),
        QStringLiteral("/usr/local/share/applications")}) {
    if (!dirs.contains(extra) && QDir(extra).exists()) {
      dirs << extra;
    }
  }
  return dirs;
}

DesktopEntry parse_desktop_file(const QString& path)
{
  DesktopEntry e;
  e.path = path;
  e.id = QFileInfo(path).fileName();

  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return e;
  }
  QTextStream in(&f);
  bool in_desktop_entry = false;
  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
      continue;
    }
    if (line.startsWith(QLatin1Char('['))) {
      in_desktop_entry = (line == QLatin1String("[Desktop Entry]"));
      continue;
    }
    if (!in_desktop_entry) {
      continue;
    }
    const int eq = line.indexOf(QLatin1Char('='));
    if (eq <= 0) {
      continue;
    }
    const QString key = line.left(eq);
    const QString val = line.mid(eq + 1);
    if (key == QLatin1String("Name") && e.name.isEmpty()) {
      e.name = val;
    } else if (key == QLatin1String("Exec") && e.exec.isEmpty()) {
      e.exec = val;
    } else if (key == QLatin1String("Icon") && e.icon.isEmpty()) {
      e.icon = val;
    } else if (key == QLatin1String("TryExec") && e.try_exec.isEmpty()) {
      e.try_exec = val;
    } else if (key == QLatin1String("MimeType")) {
      e.mime_types = val.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    } else if (key == QLatin1String("NoDisplay")) {
      e.no_display = (val.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0);
    }
  }
  return e;
}

std::vector<DesktopEntry> scan_all()
{
  std::vector<DesktopEntry> out;
  for (const QString& dir : application_dirs()) {
    const QDir d(dir);
    if (!d.exists()) {
      continue;
    }
    const QFileInfoList files =
        d.entryInfoList({QStringLiteral("*.desktop")}, QDir::Files | QDir::Readable);
    for (const QFileInfo& fi : files) {
      out.push_back(parse_desktop_file(fi.absoluteFilePath()));
    }
  }
  return out;
}

void usage(const char* argv0)
{
  std::cerr << "Usage: " << argv0 << " [options] [DESKTOP_ID…]\n"
               "  Query installed XDG .desktop application entries.\n\n"
               "Options:\n"
               "  -l, --list          List all desktop ids and names\n"
               "  -m, --mime TYPE     List apps claiming MIME type TYPE\n"
               "  -V, --version\n"
               "  -h, --help\n\n"
               "With DESKTOP_ID arguments (e.g. firefox.desktop), print details.\n";
}

void print_entry(const DesktopEntry& e)
{
  std::cout << e.id.toStdString() << '\n';
  std::cout << "  Path:      " << e.path.toStdString() << '\n';
  std::cout << "  Name:      " << e.name.toStdString() << '\n';
  std::cout << "  Exec:      " << e.exec.toStdString() << '\n';
  std::cout << "  Icon:      " << e.icon.toStdString() << '\n';
  if (!e.try_exec.isEmpty()) {
    std::cout << "  TryExec:   " << e.try_exec.toStdString() << '\n';
  }
  if (!e.mime_types.isEmpty()) {
    std::cout << "  MimeTypes: " << e.mime_types.join(QStringLiteral("; ")).toStdString() << '\n';
  }
  if (e.no_display) {
    std::cout << "  NoDisplay: true\n";
  }
  std::cout << '\n';
}

} // namespace

int main(int argc, char** argv)
{
  bool list_all = false;
  QString mime_filter;
  std::vector<QString> ids;

  for (int i = 1; i < argc; ++i) {
    const std::string_view a{argv[i]};
    if (a == "-V" || a == "--version") {
      std::cout << "dirtoo " DIRTOO_VERSION "\n";
      return 0;
    }
    if (a == "-h" || a == "--help") {
      usage(argv[0]);
      return 0;
    }
    if (a == "-l" || a == "--list") {
      list_all = true;
      continue;
    }
    if (a == "-m" || a == "--mime") {
      if (i + 1 >= argc) {
        std::cerr << "missing argument for " << a << '\n';
        return 2;
      }
      mime_filter = QString::fromUtf8(argv[++i]);
      continue;
    }
    if (a.starts_with('-')) {
      std::cerr << "unknown option: " << a << '\n';
      return 2;
    }
    ids.push_back(QString::fromUtf8(argv[i]));
  }

  QCoreApplication app(argc, argv);
  const auto entries = scan_all();

  if (list_all || (!mime_filter.isEmpty() && ids.empty())) {
    for (const DesktopEntry& e : entries) {
      if (e.no_display && mime_filter.isEmpty()) {
        continue;
      }
      if (!mime_filter.isEmpty()) {
        bool hit = false;
        for (const QString& mt : e.mime_types) {
          if (mt.compare(mime_filter, Qt::CaseInsensitive) == 0) {
            hit = true;
            break;
          }
        }
        if (!hit) {
          continue;
        }
      }
      std::cout << e.id.toStdString() << "  " << e.name.toStdString() << '\n';
    }
    return 0;
  }

  if (ids.empty()) {
    usage(argv[0]);
    return 2;
  }

  for (const QString& id : ids) {
    bool found = false;
    for (const DesktopEntry& e : entries) {
      if (e.id.compare(id, Qt::CaseInsensitive) == 0
          || e.path.endsWith(QLatin1Char('/') + id)) {
        print_entry(e);
        found = true;
        break;
      }
    }
    if (!found) {
      std::cerr << "not found: " << id.toStdString() << '\n';
    }
  }
  return 0;
}

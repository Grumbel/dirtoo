// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Resolve theme icons / MIME icons (Python dt-icon).

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QMimeDatabase>
#include <QMimeType>

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace {

void usage(const char* argv0)
{
  std::cerr << "Usage: " << argv0 << " [options] [ICON…]\n"
               "  Check whether icon theme names resolve, or list theme paths.\n\n"
               "Options:\n"
               "  -m, --mime-type   Treat arguments as MIME types; print icon names\n"
               "  --list-mime       List MIME types and their icon names\n"
               "  -V, --version\n"
               "  -h, --help\n";
}

} // namespace

int main(int argc, char** argv)
{
  bool mime_mode = false;
  bool list_mime = false;
  std::vector<QString> names;

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
    if (a == "-m" || a == "--mime-type") {
      mime_mode = true;
      continue;
    }
    if (a == "--list-mime") {
      list_mime = true;
      continue;
    }
    if (a.starts_with('-')) {
      std::cerr << "unknown option: " << a << '\n';
      return 2;
    }
    names.push_back(QString::fromUtf8(argv[i]));
  }

  // QIcon theme APIs need a QGuiApplication on some platforms.
  QGuiApplication app(argc, argv);

  if (list_mime) {
    QMimeDatabase db;
    for (const QMimeType& mt : db.allMimeTypes()) {
      std::cout << mt.name().toStdString() << "  " << mt.iconName().toStdString() << '\n';
    }
    return 0;
  }

  if (names.empty()) {
    std::cout << "Theme: " << QIcon::themeName().toStdString() << '\n';
    const QStringList paths = QIcon::themeSearchPaths();
    for (int i = 0; i < paths.size(); ++i) {
      if (i == 0) {
        std::cout << "ThemeSearchPath: " << paths[i].toStdString() << '\n';
      } else {
        std::cout << "                 " << paths[i].toStdString() << '\n';
      }
    }
    return 0;
  }

  if (mime_mode) {
    QMimeDatabase db;
    for (const QString& name : names) {
      const QMimeType mt = db.mimeTypeForName(name);
      if (!mt.isValid()) {
        std::cout << name.toStdString() << ": invalid mime-type\n";
        continue;
      }
      const QString icon = mt.iconName();
      std::cout << name.toStdString() << ": " << icon.toStdString() << "  "
                << (QIcon::hasThemeIcon(icon) ? "OK" : "FAILED") << '\n';
    }
    return 0;
  }

  for (const QString& name : names) {
    std::cout << name.toStdString() << ": "
              << (QIcon::hasThemeIcon(name) ? "OK" : "FAILED") << '\n';
  }
  return 0;
}

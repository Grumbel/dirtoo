// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Query MIME types for files (Python dt-mime).

#include <QCoreApplication>
#include <QFileInfo>
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
  std::cerr << "Usage: " << argv0 << " [options] <path>…\n"
               "  Print MIME type information for files.\n\n"
               "Options:\n"
               "  -n, --name-only   Print only the MIME type name\n"
               "  -V, --version\n"
               "  -h, --help\n";
}

} // namespace

int main(int argc, char** argv)
{
  bool name_only = false;
  std::vector<QString> paths;

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
    if (a == "-n" || a == "--name-only") {
      name_only = true;
      continue;
    }
    if (a.starts_with('-')) {
      std::cerr << "unknown option: " << a << '\n';
      return 2;
    }
    paths.push_back(QString::fromLocal8Bit(argv[i]));
  }

  if (paths.empty()) {
    usage(argv[0]);
    return 2;
  }

  QCoreApplication app(argc, argv);
  QMimeDatabase db;

  for (const QString& path : paths) {
    const QFileInfo fi(path);
    const QMimeType mt = db.mimeTypeForFile(fi);
    if (name_only) {
      std::cout << mt.name().toStdString() << '\n';
      continue;
    }
    std::cout << path.toStdString() << '\n';
    std::cout << "  name:        " << mt.name().toStdString() << '\n';
    std::cout << "  comment:     " << mt.comment().toStdString() << '\n';
    std::cout << "  icon:        " << mt.iconName().toStdString() << '\n';
    std::cout << "  genericIcon: " << mt.genericIconName().toStdString() << '\n';
    if (!mt.aliases().isEmpty()) {
      std::cout << "  aliases:     " << mt.aliases().join(QStringLiteral(", ")).toStdString()
                << '\n';
    }
    if (!mt.parentMimeTypes().isEmpty()) {
      std::cout << "  parents:     "
                << mt.parentMimeTypes().join(QStringLiteral(", ")).toStdString() << '\n';
    }
    std::cout << '\n';
  }
  return 0;
}

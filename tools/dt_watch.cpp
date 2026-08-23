// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Print directory watcher events (Python dt-watch).

#include "dirtoo/fs/location.hpp"
#include "dirtoo/watcher/directory_watcher.hpp"

#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include <iostream>
#include <string>
#include <string_view>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace {

void usage(const char* argv0)
{
  std::cerr << "Usage: " << argv0 << " [options] <directory>\n"
               "  Watch a directory and print create/remove/modify events.\n\n"
               "Options:\n"
               "  -v, --verbose   Extra messages from the watcher\n"
               "  -V, --version\n"
               "  -h, --help\n";
}

} // namespace

int main(int argc, char** argv)
{
  bool verbose = false;
  std::string directory;

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
    if (a == "-v" || a == "--verbose") {
      verbose = true;
      continue;
    }
    if (a.starts_with('-')) {
      std::cerr << "unknown option: " << a << '\n';
      return 2;
    }
    if (!directory.empty()) {
      std::cerr << "only one directory argument is supported\n";
      return 2;
    }
    directory = argv[i];
  }

  if (directory.empty()) {
    usage(argv[0]);
    return 2;
  }

  QCoreApplication app(argc, argv);

  dirtoo::watcher::DirectoryWatcher watcher;
  const auto loc = dirtoo::fs::Location::from_human(directory);
  watcher.set_location(loc);

  QObject::connect(&watcher, &dirtoo::watcher::DirectoryWatcher::entries_changed, &app,
                   [](const QStringList& created, const QStringList& removed,
                      const QStringList& modified) {
                     for (const QString& p : created) {
                       std::cout << "added " << p.toStdString() << '\n';
                     }
                     for (const QString& p : removed) {
                       std::cout << "removed " << p.toStdString() << '\n';
                     }
                     for (const QString& p : modified) {
                       std::cout << "modified " << p.toStdString() << '\n';
                     }
                     std::cout.flush();
                   });

  QObject::connect(&watcher, &dirtoo::watcher::DirectoryWatcher::directory_changed, &app,
                   [verbose] {
                     if (verbose) {
                       std::cout << "directory_changed\n";
                       std::cout.flush();
                     }
                   });

  QObject::connect(&watcher, &dirtoo::watcher::DirectoryWatcher::message, &app,
                   [verbose](const QString& text) {
                     if (verbose) {
                       std::cout << "message: " << text.toStdString() << '\n';
                       std::cout.flush();
                     }
                   });

  watcher.start();
  if (verbose) {
    std::cout << "watching " << loc.as_path().string()
              << (watcher.has_name_deltas() ? " (inotify)" : " (QFileSystemWatcher)") << '\n';
    std::cout.flush();
  }

  return app.exec();
}

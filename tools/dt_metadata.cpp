// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Show filesystem + media metadata for files (Python dt-metadata subset).

#include "dirtoo/filter/media_probe.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace fs = std::filesystem;

namespace {

void usage(const char* argv0)
{
  std::cerr << "Usage: " << argv0 << " [options] <path>…\n"
               "  Print file metadata (stat + MIME + optional media probe).\n\n"
               "Options:\n"
               "  -r, --recursive   Recurse into directories\n"
               "  -V, --version\n"
               "  -h, --help\n";
}

void print_one(const QString& path)
{
  const QFileInfo fi(path);
  std::cout << path.toStdString() << '\n';
  if (!fi.exists()) {
    std::cout << "    error: path does not exist\n\n";
    return;
  }

  std::cout << "    basename:  " << fi.fileName().toStdString() << '\n';
  std::cout << "    size:      " << fi.size() << '\n';
  std::cout << "    is_dir:    " << (fi.isDir() ? "true" : "false") << '\n';
  std::cout << "    is_file:   " << (fi.isFile() ? "true" : "false") << '\n';
  std::cout << "    is_symlink:" << (fi.isSymLink() ? "true" : "false") << '\n';
  std::cout << "    mtime:     "
            << fi.lastModified().toString(Qt::ISODate).toStdString() << '\n';
  if (fi.isSymLink()) {
    std::cout << "    target:    " << fi.symLinkTarget().toStdString() << '\n';
  }

  QMimeDatabase db;
  const QMimeType mt = db.mimeTypeForFile(fi);
  std::cout << "    mime:      " << mt.name().toStdString() << '\n';
  std::cout << "    mime_icon: " << mt.iconName().toStdString() << '\n';

  if (fi.isFile()) {
    if (const auto media = dirtoo::filter::probe_media(fs::path{path.toStdString()})) {
      if (media->width) {
        std::cout << "    width:     " << *media->width << '\n';
      }
      if (media->height) {
        std::cout << "    height:    " << *media->height << '\n';
      }
      if (media->duration_ms) {
        std::cout << "    duration_ms: " << *media->duration_ms << '\n';
      }
      if (media->framerate) {
        std::cout << "    framerate: " << *media->framerate << '\n';
      }
      if (media->pages) {
        std::cout << "    pages:     " << *media->pages << '\n';
      }
    }
  }
  std::cout << '\n';
}

void walk(const QString& path, bool recursive)
{
  const QFileInfo fi(path);
  if (recursive && fi.isDir() && !fi.isSymLink()) {
    const QDir dir(path);
    const auto entries =
        dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable);
    for (const QString& name : entries) {
      walk(dir.absoluteFilePath(name), true);
    }
    return;
  }
  print_one(path);
}

} // namespace

int main(int argc, char** argv)
{
  bool recursive = false;
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
    if (a == "-r" || a == "--recursive") {
      recursive = true;
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
  for (const QString& p : paths) {
    walk(p, recursive);
  }
  return 0;
}

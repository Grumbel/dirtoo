// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/archive/archive_index.hpp"

#include <iostream>
#include <string_view>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace {

void usage()
{
  std::cerr << "usage: dt-archiveinfo [--file-count] [--list] <archive> [archive…]\n"
               "  Print archive table-of-contents summary (bsdtar/unzip/7z).\n"
               "  Python: programs/archiveinfo.py.\n";
}

} // namespace

int main(int argc, char* argv[])
{
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--version" || std::string_view(argv[i]) == "-V") {
      std::cout << "dirtoo " DIRTOO_VERSION "\n";
      return 0;
    }
  }

  bool file_count_only = false;
  bool list_members = false;
  int argi = 1;
  while (argi < argc && std::string_view(argv[argi]).starts_with('-')) {
    const std::string_view a{argv[argi]};
    if (a == "--file-count") {
      file_count_only = true;
    } else if (a == "--list" || a == "-l") {
      list_members = true;
    } else if (a == "--help" || a == "-h") {
      usage();
      return 0;
    } else {
      std::cerr << "unknown option: " << a << '\n';
      usage();
      return 2;
    }
    ++argi;
  }

  if (argi >= argc) {
    usage();
    return 2;
  }

  int failures = 0;
  for (; argi < argc; ++argi) {
    const std::filesystem::path path{argv[argi]};
    auto entries = dirtoo::archive::list_archive_entries(path);
    if (!entries) {
      std::cerr << path.string() << ": " << entries.error() << '\n';
      ++failures;
      continue;
    }

    std::uint64_t files = 0;
    std::uint64_t dirs = 0;
    for (const auto& e : *entries) {
      if (e.is_directory) {
        ++dirs;
      } else {
        ++files;
      }
    }

    if (file_count_only) {
      std::cout << path.string() << '\t' << files << '\n';
      continue;
    }

    std::cout << path.string() << ": " << files << " file(s), " << dirs << " dir(s), "
              << entries->size() << " entr(y/ies)\n";
    if (list_members) {
      for (const auto& e : *entries) {
        std::cout << (e.is_directory ? "d " : "f ") << e.path.generic_string();
        if (!e.is_directory && e.size > 0) {
          std::cout << "  (" << e.size << " B)";
        }
        std::cout << '\n';
      }
    }
  }
  return failures == 0 ? 0 : 1;
}

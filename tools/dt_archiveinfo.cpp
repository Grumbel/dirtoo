// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/archive/archive_index.hpp"
#include "json_util.hpp"

#include <iostream>
#include <string_view>
#include <vector>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace {

void usage()
{
  std::cerr << "usage: dt-archiveinfo [options] <archive> [archive…]\n"
               "  --file-count     Print path and file count only\n"
               "  -l, --list       List member paths\n"
               "  -j, --json       JSON array of archive summaries\n"
               "  -V, --version    Print version and exit\n"
               "  -h, --help\n";
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
  bool json = false;
  std::vector<std::filesystem::path> archives;

  for (int i = 1; i < argc; ++i) {
    const std::string_view a{argv[i]};
    if (a == "--file-count") {
      file_count_only = true;
      continue;
    }
    if (a == "--list" || a == "-l") {
      list_members = true;
      continue;
    }
    if (a == "-j" || a == "--json") {
      json = true;
      continue;
    }
    if (a == "--help" || a == "-h") {
      usage();
      return 0;
    }
    if (a.starts_with('-')) {
      std::cerr << "unknown option: " << a << '\n';
      usage();
      return 2;
    }
    archives.emplace_back(argv[i]);
  }

  if (archives.empty()) {
    usage();
    return 2;
  }

  int failures = 0;
  if (json) {
    std::cout << '[';
    bool first_arch = true;
    for (const auto& path : archives) {
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
      if (!first_arch) {
        std::cout << ',';
      }
      first_arch = false;
      std::cout << '{';
      bool first = true;
      dtjson::write_kv_string(std::cout, "path", path.string(), first);
      dtjson::write_kv_uint(std::cout, "files", files, first);
      dtjson::write_kv_uint(std::cout, "dirs", dirs, first);
      dtjson::write_kv_uint(std::cout, "entries", entries->size(), first);
      if (list_members) {
        if (!first) {
          std::cout << ',';
        }
        first = false;
        dtjson::write_escaped(std::cout, "members");
        std::cout << ":[";
        bool first_m = true;
        for (const auto& e : *entries) {
          if (!first_m) {
            std::cout << ',';
          }
          first_m = false;
          std::cout << '{';
          bool mf = true;
          dtjson::write_kv_string(std::cout, "path", e.path.generic_string(), mf);
          dtjson::write_kv_bool(std::cout, "directory", e.is_directory, mf);
          if (!e.is_directory) {
            dtjson::write_kv_uint(std::cout, "size", e.size, mf);
          }
          std::cout << '}';
        }
        std::cout << ']';
      }
      std::cout << '}';
    }
    std::cout << "]\n";
    return failures == 0 ? 0 : 1;
  }

  for (const auto& path : archives) {
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

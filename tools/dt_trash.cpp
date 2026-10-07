// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/trash.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace {

void usage()
{
  std::cerr
      << "Usage: dt-trash [options] <path>...      move to the trash\n"
         "       dt-trash --list                   list trashed items\n"
         "       dt-trash --restore <name|path>... put items back\n"
         "       dt-trash --empty                  delete everything in the trash\n"
         "\n"
         "Implements the freedesktop.org Trash specification.\n"
         "\n"
         "Options:\n"
         "  -l, --list      List the trash (original path, deletion date, name)\n"
         "  -r, --restore   Restore items; each argument is a trash name or the original path\n"
         "  -e, --empty     Permanently delete all trashed items\n"
         "  -n, --dry-run   Show what would happen\n"
         "  -v, --verbose   Print each item\n"
         "  -V, --version   Print version and exit\n"
         "  -h, --help      Show this help\n";
}

} // namespace

int main(int argc, char* argv[])
{
  enum class Mode { Trash, List, Restore, Empty } mode = Mode::Trash;
  dirops::Options opt;
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    const std::string_view a{argv[i]};
    if (a == "--") {
      for (++i; i < argc; ++i) {
        args.emplace_back(argv[i]);
      }
      break;
    }
    if (a == "-V" || a == "--version") {
      std::cout << "dirtoo " DIRTOO_VERSION "\n";
      return 0;
    }
    if (a == "-h" || a == "--help") {
      usage();
      return 0;
    }
    if (a == "-l" || a == "--list") {
      mode = Mode::List;
    } else if (a == "-r" || a == "--restore") {
      mode = Mode::Restore;
    } else if (a == "-e" || a == "--empty") {
      mode = Mode::Empty;
    } else if (a == "-n" || a == "--dry-run") {
      opt.dry_run = true;
    } else if (a == "-v" || a == "--verbose") {
      opt.verbose = true;
    } else if (a.starts_with('-') && a.size() > 1) {
      std::cerr << "unknown option: " << a << '\n';
      usage();
      return 2;
    } else {
      args.emplace_back(a);
    }
  }

  int failures = 0;
  switch (mode) {
  case Mode::Trash: {
    if (args.empty()) {
      usage();
      return 2;
    }
    for (const auto& path : args) {
      auto r = dirops::trash_path(path, {}, opt);
      if (!r) {
        std::cerr << path << ": " << r.error().to_string() << '\n';
        ++failures;
      } else if (opt.verbose || opt.dry_run) {
        for (const auto& item : r->items) {
          std::cout << (opt.dry_run ? "trash " : "trashed ") << item.source.string();
          if (!item.destination.empty()) {
            std::cout << " -> " << item.destination.string();
          }
          std::cout << '\n';
        }
      }
    }
    break;
  }
  case Mode::List: {
    auto items = dirops::list_trash();
    std::sort(items.begin(), items.end(),
              [](const auto& a, const auto& b) { return a.deletion_date < b.deletion_date; });
    for (const auto& e : items) {
      std::cout << e.deletion_date << '\t' << e.original_path.string() << '\t' << e.name << '\n';
    }
    break;
  }
  case Mode::Restore: {
    if (args.empty()) {
      usage();
      return 2;
    }
    const auto items = dirops::list_trash();
    for (const auto& arg : args) {
      std::vector<const dirops::TrashEntry*> matches;
      for (const auto& e : items) {
        if (e.name == arg || e.original_path == std::filesystem::path{arg}) {
          matches.push_back(&e);
        }
      }
      if (matches.empty()) {
        std::cerr << arg << ": not found in the trash\n";
        ++failures;
        continue;
      }
      if (matches.size() > 1) {
        std::cerr << arg << ": ambiguous (" << matches.size()
                  << " trashed items match); use the trash name from --list\n";
        ++failures;
        continue;
      }
      auto r = dirops::restore_trash_entry(*matches.front(), opt);
      if (!r) {
        std::cerr << arg << ": " << r.error().to_string() << '\n';
        ++failures;
      } else if (opt.verbose || opt.dry_run) {
        std::cout << (opt.dry_run ? "restore " : "restored ")
                  << matches.front()->original_path.string() << '\n';
      }
    }
    break;
  }
  case Mode::Empty: {
    auto r = dirops::empty_trash({}, opt);
    if (!r) {
      std::cerr << r.error().to_string() << '\n';
      ++failures;
    } else if (opt.verbose) {
      for (const auto& item : r->items) {
        std::cout << "emptied " << item.source.string() << '\n';
      }
    }
    break;
  }
  }
  return failures == 0 ? 0 : 1;
}

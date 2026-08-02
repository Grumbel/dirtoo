// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/ops.hpp"

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
  std::cerr << "usage: dt-rm [--dry-run] [--verbose] <path> [path…]\n"
               "  Remove files or directories (recursive). Python: programs/rmdir.py + delete.\n";
}

} // namespace

int main(int argc, char* argv[])
{
  for (int i = 1; i < argc; ++i) {
    const std::string_view a{argv[i]};
    if (a == "--version" || a == "-V") {
      std::cout << "dirtoo " DIRTOO_VERSION "\n";
      return 0;
    }
  }

  dirops::Options opt;
  int argi = 1;
  while (argi < argc && std::string_view(argv[argi]).starts_with('-')) {
    const std::string_view a{argv[argi]};
    if (a == "--dry-run") {
      opt.dry_run = true;
    } else if (a == "--verbose" || a == "-v") {
      opt.verbose = true;
    } else if (a == "--help" || a == "-h") {
      usage();
      return 0;
    } else if (a == "--") {
      ++argi;
      break;
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
    auto result = dirops::remove_path(argv[argi], opt);
    if (!result) {
      std::cerr << argv[argi] << ": " << result.error().to_string() << '\n';
      ++failures;
      continue;
    }
    if (opt.dry_run || opt.verbose) {
      for (const auto& item : result->items) {
        std::cout << (item.skipped ? "skip " : "rm ") << item.source.string() << '\n';
      }
    }
  }
  return failures == 0 ? 0 : 1;
}

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
  std::cerr
      << "Usage: dt-rm [options] <path> [path…]\n"
         "\n"
         "Remove files or directories. Directories are removed recursively\n"
         "(equivalent to rm -rf for each argument).\n"
         "\n"
         "Options:\n"
         "  -n, --dry-run                List paths that would be removed\n"
         "  -v, --verbose                Print each removed path\n"
         "  -V, --version                Print version and exit\n"
         "  -h, --help                   Show this help\n"
         "\n"
         "Examples:\n"
         "  dt-rm obsolete.txt\n"
         "  dt-rm -v build/ tmp/cache\n"
         "  dt-rm --dry-run /tmp/scratch\n";
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
    if (a == "--dry-run" || a == "-n") {
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

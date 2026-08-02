// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/ops.hpp"

#include <iostream>
#include <string_view>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace {

void usage()
{
  std::cerr << "usage: dt-mkfile [--dry-run] <path> [path…]\n"
               "  Create empty regular file(s). Fails if a path already exists.\n";
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

  dirops::Options opt;
  int argi = 1;
  while (argi < argc && std::string_view(argv[argi]).starts_with('-')) {
    const std::string_view a{argv[argi]};
    if (a == "--dry-run") {
      opt.dry_run = true;
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
    auto result = dirops::create_file(argv[argi], opt);
    if (!result) {
      std::cerr << argv[argi] << ": " << result.error().to_string() << '\n';
      ++failures;
      continue;
    }
    if (opt.dry_run) {
      std::cout << "mkfile " << argv[argi] << '\n';
    }
  }
  return failures == 0 ? 0 : 1;
}

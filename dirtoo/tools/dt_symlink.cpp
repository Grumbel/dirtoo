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
  std::cerr << "usage: dt-symlink [--dry-run] <target> <link-path>\n"
               "  Create a symbolic link at <link-path> pointing to <target>.\n";
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

  if (argc - argi != 2) {
    usage();
    return 2;
  }

  auto result = dirops::create_symlink(argv[argi], argv[argi + 1], opt);
  if (!result) {
    std::cerr << result.error().to_string() << '\n';
    return 1;
  }
  if (opt.dry_run) {
    std::cout << "symlink " << argv[argi] << " -> " << argv[argi + 1] << '\n';
  }
  return 0;
}

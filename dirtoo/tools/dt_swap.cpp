// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cli_common.hpp"

#include <iostream>
#include <string_view>

namespace {

void usage()
{
  std::cerr << "usage: dt-swap [options] <file1> <file2>\n"
               "  Swap the names of two files (same filesystem).\n"
               "  -n, --dry-run    Do not execute\n"
               "  -v, --verbose    Print actions\n"
               "  -h, --help\n";
}

} // namespace

int main(int argc, char* argv[])
{
  for (int i = 1; i < argc; ++i) {
    if (dtcli::is_version_flag(argv[i])) {
      dtcli::print_version();
      return 0;
    }
  }

  dirops::Options opt;
  const char* a_path = nullptr;
  const char* b_path = nullptr;

  for (int i = 1; i < argc; ++i) {
    const std::string_view a{argv[i]};
    if (a == "-h" || a == "--help") {
      usage();
      return 0;
    }
    if (a == "-n" || a == "--dry-run") {
      opt.dry_run = true;
      continue;
    }
    if (a == "-v" || a == "--verbose") {
      opt.verbose = true;
      continue;
    }
    if (a.starts_with('-')) {
      std::cerr << "dt-swap: unknown option: " << a << '\n';
      usage();
      return 2;
    }
    if (a_path == nullptr) {
      a_path = argv[i];
    } else if (b_path == nullptr) {
      b_path = argv[i];
    } else {
      usage();
      return 2;
    }
  }

  if (a_path == nullptr || b_path == nullptr) {
    usage();
    return 2;
  }

  auto result = dirops::swap_names(a_path, b_path, opt);
  if (!result) {
    std::cerr << result.error().to_string() << '\n';
    return 1;
  }
  if (opt.dry_run || opt.verbose) {
    std::cout << "swap " << a_path << " <-> " << b_path << '\n';
  }
  return 0;
}

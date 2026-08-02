// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cli_common.hpp"

#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

void usage()
{
  std::cerr << "usage: dt-mkdir [options] <path> [path…]\n"
               "  -p, --parents     Create parent directories as needed\n"
               "  -n, --dry-run\n"
               "  -v, --verbose\n"
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
  bool parents = false;
  std::vector<fs::path> paths;

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
    if (a == "-p" || a == "--parents") {
      parents = true;
      continue;
    }
    if (a.starts_with('-')) {
      std::cerr << "dt-mkdir: unknown option: " << a << '\n';
      usage();
      return 2;
    }
    paths.emplace_back(argv[i]);
  }

  if (paths.empty()) {
    usage();
    return 2;
  }

  int failures = 0;
  for (const auto& path : paths) {
    if (parents) {
      if (opt.dry_run) {
        std::cout << "mkdir -p " << path.string() << '\n';
        continue;
      }
      std::error_code ec;
      fs::create_directories(path, ec);
      if (ec) {
        std::cerr << path.string() << ": " << ec.message() << '\n';
        ++failures;
        continue;
      }
      if (opt.verbose) {
        std::cout << "mkdir " << path.string() << '\n';
      }
      continue;
    }
    auto result = dirops::create_directory(path, opt);
    if (!result) {
      std::cerr << path.string() << ": " << result.error().to_string() << '\n';
      ++failures;
      continue;
    }
    if (opt.dry_run || opt.verbose) {
      std::cout << "mkdir " << path.string() << '\n';
    }
  }
  return failures == 0 ? 0 : 1;
}

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cli_common.hpp"

#include <iostream>
#include <string_view>

namespace {

void usage()
{
  std::cerr << "usage: dt-rename [options] <from> <to>\n"
               "  -n, --dry-run\n"
               "  -v, --verbose\n"
               "  -Y, --always / -N, --never / --conflict=…\n"
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
  const char* from = nullptr;
  const char* to = nullptr;

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
    if (a == "-Y" || a == "--always") {
      opt.conflict = dirops::ConflictPolicy::Overwrite;
      continue;
    }
    if (a == "-N" || a == "--never") {
      opt.conflict = dirops::ConflictPolicy::Skip;
      continue;
    }
    if (a.starts_with("--conflict=")) {
      opt.conflict = dtcli::parse_conflict(a.substr(11));
      continue;
    }
    if (a.starts_with('-')) {
      std::cerr << "dt-rename: unknown option: " << a << '\n';
      usage();
      return 2;
    }
    if (from == nullptr) {
      from = argv[i];
    } else if (to == nullptr) {
      to = argv[i];
    } else {
      usage();
      return 2;
    }
  }

  if (from == nullptr || to == nullptr) {
    usage();
    return 2;
  }

  auto result = dirops::rename_path(from, to, opt);
  if (!result) {
    std::cerr << result.error().to_string() << '\n';
    return 1;
  }
  dtcli::print_result_items(*result, "rename", opt.verbose, opt.dry_run);
  return 0;
}

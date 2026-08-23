// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cli_common.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

void usage()
{
  std::cerr
      << "Usage:\n"
         "  dt-copy [options] <from> <to>\n"
         "  dt-copy [options] -t <dir> <file> [file…]\n"
         "\n"
         "Copy files or directory trees. Symlinks are copied as symlinks.\n"
         "\n"
         "Modes:\n"
         "  <from> <to>                  Copy one source to an explicit destination path.\n"
         "                               If <to> is an existing directory, the source\n"
         "                               basename is placed inside it.\n"
         "  -t DIR <file>…               Copy each source into DIR (basename only).\n"
         "\n"
         "Options:\n"
         "  -t, --target-directory DIR   Destination directory for multi-source mode\n"
         "  -n, --dry-run                Print planned copies; do not write\n"
         "  -v, --verbose                Print each successful copy\n"
         "  -Y, --always                 On conflict, overwrite the destination\n"
         "  -N, --never                  On conflict, skip the source\n"
         "  --conflict=POLICY            fail|overwrite|rename|skip (default: fail)\n"
         "                                 rename → unique name like file (2).txt\n"
         "  -V, --version                Print version and exit\n"
         "  -h, --help                   Show this help\n"
         "\n"
         "Examples:\n"
         "  dt-copy photo.jpg /tmp/photo.jpg\n"
         "  dt-copy -t /tmp/out a.png b.png\n"
         "  dt-copy --conflict=rename tree/ backup/tree\n";
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
  fs::path target_dir;
  bool have_target = false;
  std::vector<fs::path> files;

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
    if (a == "-t" || a == "--target-directory") {
      if (i + 1 >= argc) {
        std::cerr << "dt-copy: missing argument for " << a << '\n';
        return 2;
      }
      target_dir = argv[++i];
      have_target = true;
      continue;
    }
    if (a.starts_with('-')) {
      std::cerr << "dt-copy: unknown option: " << a << '\n';
      usage();
      return 2;
    }
    files.emplace_back(argv[i]);
  }

  if (have_target) {
    if (files.empty()) {
      usage();
      return 2;
    }
    std::error_code ec;
    if (!fs::is_directory(target_dir, ec) || ec) {
      std::cerr << "dt-copy: target directory does not exist: " << target_dir.string() << '\n';
      return 1;
    }
    int failures = 0;
    for (const auto& src : files) {
      const fs::path dest = dtcli::dest_under_target(src, target_dir, /*relative=*/false);
      auto result = dirops::copy_path(src, dest, opt);
      if (!result) {
        std::cerr << result.error().to_string() << '\n';
        ++failures;
        continue;
      }
      dtcli::print_result_items(*result, "copy", opt.verbose, opt.dry_run);
    }
    return failures == 0 ? 0 : 1;
  }

  if (files.size() != 2) {
    usage();
    return 2;
  }
  auto result = dirops::copy_path(files[0], files[1], opt);
  if (!result) {
    std::cerr << result.error().to_string() << '\n';
    return 1;
  }
  dtcli::print_result_items(*result, "copy", opt.verbose, opt.dry_run);
  return 0;
}

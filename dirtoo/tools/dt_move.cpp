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
      << "Usage: dt-move [options] -t <dir> <file> [file…]\n"
         "\n"
         "Move one or more files or directories into a target directory.\n"
         "The two-argument form `dt-move <from> <to>` is not supported; always\n"
         "pass -t/--target-directory. For a single-file rename/move to a new path\n"
         "use dt-rename instead.\n"
         "\n"
         "Required:\n"
         "  -t, --target-directory DIR   Destination directory (must exist)\n"
         "\n"
         "Options:\n"
         "  -R, --relative               Preserve path prefix under DIR\n"
         "                               (e.g. a/b.txt → DIR/a/b.txt; creates parents)\n"
         "  -n, --dry-run                Print planned moves; do not touch the filesystem\n"
         "  -v, --verbose                Print each successful move\n"
         "  -Y, --always                 On name conflict, overwrite the destination\n"
         "  -N, --never                  On name conflict, skip the source\n"
         "  --conflict=POLICY            Conflict policy when the destination exists:\n"
         "                                 fail       — error and abort that item (default)\n"
         "                                 overwrite  — replace existing (same as -Y)\n"
         "                                 rename     — unique name, e.g. file (2).txt\n"
         "                                 skip       — leave destination unchanged (same as -N)\n"
         "  -V, --version                Print version and exit\n"
         "  -h, --help                   Show this help\n"
         "\n"
         "Examples:\n"
         "  dt-move -t /tmp/out photo.jpg notes.txt\n"
         "  dt-move -R -t /backup home/user/docs/a.pdf\n"
         "  dt-move -n -v --conflict=rename -t ./dest ./*.png\n";
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
  bool relative = false;
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
    if (a == "-R" || a == "--relative") {
      relative = true;
      continue;
    }
    if (a.starts_with("--conflict=")) {
      opt.conflict = dtcli::parse_conflict(a.substr(11));
      continue;
    }
    if (a == "-t" || a == "--target-directory") {
      if (i + 1 >= argc) {
        std::cerr << "dt-move: missing argument for " << a << '\n';
        return 2;
      }
      target_dir = argv[++i];
      have_target = true;
      continue;
    }
    if (a.starts_with('-')) {
      std::cerr << "dt-move: unknown option: " << a << '\n';
      usage();
      return 2;
    }
    files.emplace_back(argv[i]);
  }

  if (!have_target) {
    std::cerr << "dt-move: -t/--target-directory is required\n\n";
    usage();
    return 2;
  }
  if (files.empty()) {
    std::cerr << "dt-move: at least one source path is required\n\n";
    usage();
    return 2;
  }

  std::error_code ec;
  if (!fs::is_directory(target_dir, ec) || ec) {
    std::cerr << "dt-move: target directory does not exist: " << target_dir.string() << '\n';
    return 1;
  }

  int failures = 0;
  for (const auto& src : files) {
    const fs::path dest = dtcli::dest_under_target(src, target_dir, relative);
    if (relative) {
      std::error_code mec;
      fs::create_directories(dest.parent_path(), mec);
    }
    auto result = dirops::move_path(src, dest, opt);
    if (!result) {
      std::cerr << result.error().to_string() << '\n';
      ++failures;
      continue;
    }
    dtcli::print_result_items(*result, "move", opt.verbose, opt.dry_run);
  }
  return failures == 0 ? 0 : 1;
}

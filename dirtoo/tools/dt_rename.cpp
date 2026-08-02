// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cli_common.hpp"

#include <iostream>
#include <string_view>

namespace {

void usage()
{
  std::cerr
      << "Usage: dt-rename [options] <from> <to>\n"
         "\n"
         "Rename or move a single path to a new path (same or different directory).\n"
         "Unlike dt-move, this accepts an explicit destination path rather than a\n"
         "target directory.\n"
         "\n"
         "Arguments:\n"
         "  <from>                       Existing source path\n"
         "  <to>                         New path (file or directory name)\n"
         "\n"
         "Options:\n"
         "  -n, --dry-run                Print the planned rename; do not change the FS\n"
         "  -v, --verbose                Print the rename when it succeeds\n"
         "  -Y, --always                 If <to> exists, overwrite it\n"
         "  -N, --never                  If <to> exists, skip (no error)\n"
         "  --conflict=POLICY            What to do when the destination already exists:\n"
         "                                 fail       — error (default)\n"
         "                                 overwrite  — remove destination then rename\n"
         "                                 rename     — pick a free name: stem (N).ext\n"
         "                                              e.g. report.pdf → report (2).pdf\n"
         "                                 skip       — leave both paths unchanged\n"
         "  -V, --version                Print version and exit\n"
         "  -h, --help                   Show this help\n"
         "\n"
         "Conflict resolution details:\n"
         "  fail        Returns an error if <to> already exists; nothing is changed.\n"
         "  overwrite   Deletes the existing <to> (recursively if a directory), then\n"
         "              renames <from> onto that path. Cross-device moves are not done\n"
         "              here — use dt-move for cross-filesystem transfers.\n"
         "  rename      Leaves the existing <to> alone and renames <from> to the first\n"
         "              free name in the destination directory matching\n"
         "              \"<stem> (N)<ext>\" with N starting at 2.\n"
         "  skip        Treats a conflict as success with a skipped item; <from> stays.\n"
         "\n"
         "Examples:\n"
         "  dt-rename old.txt new.txt\n"
         "  dt-rename --conflict=rename photo.jpg photo.jpg\n"
         "  dt-rename -n -v notes.md archive/notes-2024.md\n";
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

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/ops.hpp"

#include <iostream>
#include <string_view>

namespace {

dirops::ConflictPolicy parse_conflict(std::string_view s)
{
  if (s == "overwrite") return dirops::ConflictPolicy::Overwrite;
  if (s == "rename") return dirops::ConflictPolicy::Rename;
  if (s == "skip") return dirops::ConflictPolicy::Skip;
  return dirops::ConflictPolicy::Fail;
}

void usage()
{
  std::cerr << "usage: dt-move [--dry-run] [--conflict=fail|overwrite|rename|skip] <from> <to>\n";
}

} // namespace

int main(int argc, char* argv[])
{
  dirops::Options opt;
  int argi = 1;
  while (argi < argc && std::string_view(argv[argi]).starts_with("--")) {
    std::string_view a = argv[argi];
    if (a == "--dry-run") {
      opt.dry_run = true;
    } else if (a.starts_with("--conflict=")) {
      opt.conflict = parse_conflict(a.substr(11));
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

  auto result = dirops::move_path(argv[argi], argv[argi + 1], opt);
  if (!result) {
    std::cerr << result.error().to_string() << '\n';
    return 1;
  }
  return 0;
}

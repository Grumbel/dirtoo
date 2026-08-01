// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/ops.hpp"

#include <iostream>
#include <string>
#include <string_view>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

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
  std::cerr << "usage: dt-copy [--dry-run] [--conflict=fail|overwrite|rename|skip] <from> <to>\n";
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

  auto result = dirops::copy_path(argv[argi], argv[argi + 1], opt);
  if (!result) {
    std::cerr << result.error().to_string() << '\n';
    return 1;
  }
  if (opt.dry_run) {
    for (const auto& item : result->items) {
      if (item.skipped) {
        std::cout << "skip " << item.source.string() << '\n';
      } else {
        std::cout << "copy " << item.source.string() << " -> " << item.destination.string() << '\n';
      }
    }
  }
  return 0;
}

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/parser.hpp"
#include "dirtoo/filter/filter_item.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static void usage(const char* argv0)
{
  std::cerr << "Usage: " << argv0 << " <expression> [directory]\n\n"
            << dirtoo::filter::filter_help_text();
}

int main(int argc, char** argv)
{
  if (argc < 2) {
    usage(argv[0]);
    return 2;
  }
  const std::string expr = argv[1];
  if (expr == "-h" || expr == "--help") {
    usage(argv[0]);
    return 0;
  }

  const auto parsed = dirtoo::filter::parse_filter(expr);
  if (!parsed) {
    std::cerr << "parse error at " << parsed.error().position << ": "
              << parsed.error().message << '\n';
    return 1;
  }

  fs::path dir = ".";
  if (argc >= 3) {
    dir = argv[2];
  }

  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(dir, ec)) {
    if (ec) {
      break;
    }
    dirtoo::filter::FilterItem item;
    item.name = entry.path().filename().string();
    item.path = entry.path();
    item.is_directory = entry.is_directory(ec);
    if (!item.is_directory) {
      item.size = entry.file_size(ec);
      if (ec) {
        item.size = 0;
        ec.clear();
      }
    }
    if ((*parsed)->matches(item)) {
      std::cout << entry.path().string() << '\n';
    }
  }
  return 0;
}

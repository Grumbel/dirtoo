// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/parser.hpp"
#include "dirtoo/filter/search.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

static void usage(const char* argv0)
{
  std::cerr
      << "Usage: " << argv0
      << " [options] <expression> [directory]\n\n"
      << "Options:\n"
      << "  -r, --recursive       Search directories recursively\n"
      << "  -d, --max-depth N     Limit recursion depth (requires -r; 0 = one level)\n"
      << "  -a, --all             Include hidden (dot) files\n"
      << "  -0, --null            Separate paths with NUL instead of newline\n"
      << "  -h, --help            Show this help\n\n"
      << dirtoo::filter::filter_help_text();
}

int main(int argc, char** argv)
{
  bool recursive = false;
  int max_depth = -1;
  bool show_hidden = false;
  bool null_sep = false;
  std::vector<std::string> positional;

  for (int i = 1; i < argc; ++i) {
    const std::string_view a{argv[i]};
    if (a == "-h" || a == "--help") {
      usage(argv[0]);
      return 0;
    }
    if (a == "-r" || a == "--recursive") {
      recursive = true;
      continue;
    }
    if (a == "-a" || a == "--all") {
      show_hidden = true;
      continue;
    }
    if (a == "-0" || a == "--null") {
      null_sep = true;
      continue;
    }
    if (a == "-d" || a == "--max-depth") {
      if (i + 1 >= argc) {
        std::cerr << "missing argument for " << a << '\n';
        return 2;
      }
      max_depth = std::atoi(argv[++i]);
      recursive = true;
      continue;
    }
    if (a.starts_with("--max-depth=")) {
      max_depth = std::atoi(std::string(a.substr(12)).c_str());
      recursive = true;
      continue;
    }
    if (a.starts_with('-')) {
      std::cerr << "unknown option: " << a << '\n';
      usage(argv[0]);
      return 2;
    }
    positional.emplace_back(a);
  }

  if (positional.empty()) {
    usage(argv[0]);
    return 2;
  }

  const std::string& expr = positional[0];
  fs::path dir = ".";
  if (positional.size() >= 2) {
    dir = positional[1];
  }

  const auto parsed = dirtoo::filter::parse_filter(expr);
  if (!parsed) {
    std::cerr << "parse error at " << parsed.error().position << ": "
              << parsed.error().message << '\n';
    return 1;
  }

  dirtoo::filter::SearchOptions opt;
  opt.show_hidden = show_hidden;
  opt.max_depth = recursive ? max_depth : 0;

  const char sep = null_sep ? '\0' : '\n';
  const auto stats = dirtoo::filter::search_directory(
      dir, **parsed, opt, [&](const dirtoo::filter::FilterItem& item) {
        std::cout << item.path.string() << sep;
      });

  if (stats.errors > 0 && stats.matched == 0 && stats.visited == 0) {
    std::cerr << "search failed (path missing or not a directory?)\n";
    return 1;
  }
  return 0;
}

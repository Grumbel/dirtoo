// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Recursive find with dirtoo filter expressions (Python dt-find / dt-search).
/// Unlike classic `find`, matching uses the dirtoo filter DSL (name:, size:,
/// type:, media predicates, …) rather than only -name/-type primaries.
///
/// dt-find:  positional DIRECTORY args; filter via -f/--filter.
/// dt-search: positional QUERY words form the filter (SimpleFilter parity);
///            directories via -d/--directory (default ".").

#include "dirtoo/filter/match_func.hpp"
#include "dirtoo/filter/parser.hpp"
#include "dirtoo/filter/search.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace fs = std::filesystem;

namespace {

[[nodiscard]] bool is_search_mode(const char* argv0)
{
  if (argv0 == nullptr) {
    return false;
  }
  std::string_view name{argv0};
  const auto slash = name.find_last_of("/\\");
  if (slash != std::string_view::npos) {
    name = name.substr(slash + 1);
  }
  return name == "dt-search" || name.starts_with("dt-search.");
}

void usage(const char* argv0, bool search_mode)
{
  std::cerr
      << "Usage: " << argv0
      << (search_mode ? " [options] [QUERY…]\n\n" : " [options] [directory…]\n\n")
      << "Find files using the dirtoo filter language (not POSIX find primaries).\n";
  if (search_mode) {
    std::cerr
        << "dt-search: positional arguments are the filter query (e.g. dt-search foo).\n"
        << "Default search root is the current directory; use -d DIR to override.\n";
  } else {
    std::cerr
        << "dt-find: positional arguments are directories to search (default: .).\n"
        << "Filter expression via -f/--filter (default: match all).\n";
  }
  std::cerr
      << "Also installed as " << (search_mode ? "dt-find" : "dt-search")
      << " with different positional argument meaning.\n\n"
      << "Options:\n"
      << "  -f, --filter EXPR     Filter expression"
      << (search_mode ? " (overrides QUERY)\n" : " (default: match all)\n")
      << "  -d, --directory DIR   Search root (repeatable; dt-search; default: .)\n"
      << "  -D, --maxdepth N      Maximum recursion depth (0 = only root entries)\n"
      << "  -a, --all             Include hidden (dot) files\n"
      << "  -0, --null            NUL-separate paths\n"
      << "  -l, --list            Verbose: type and size before path\n"
      << "  -V, --version\n"
      << "  -h, --help\n\n"
      << dirtoo::filter::filter_help_text();
}

} // namespace

int main(int argc, char** argv)
{
  const bool search_mode = is_search_mode(argc > 0 ? argv[0] : nullptr);

  std::string filter_expr;
  int max_depth = -1;
  bool show_hidden = false;
  bool null_sep = false;
  bool list_mode = false;
  std::vector<std::string> directories;
  std::vector<std::string> query_words;

  for (int i = 1; i < argc; ++i) {
    const std::string_view a{argv[i]};
    if (a == "-V" || a == "--version") {
      std::cout << "dirtoo " DIRTOO_VERSION "\n";
      return 0;
    }
    if (a == "-h" || a == "--help") {
      usage(argv[0], search_mode);
      return 0;
    }
    if (a == "-a" || a == "--all") {
      show_hidden = true;
      continue;
    }
    if (a == "-0" || a == "--null") {
      null_sep = true;
      continue;
    }
    if (a == "-l" || a == "--list") {
      list_mode = true;
      continue;
    }
    if (a == "-f" || a == "--filter") {
      if (i + 1 >= argc) {
        std::cerr << "missing argument for " << a << '\n';
        return 2;
      }
      filter_expr = argv[++i];
      continue;
    }
    if (a == "-d" || a == "--directory") {
      if (i + 1 >= argc) {
        std::cerr << "missing argument for " << a << '\n';
        return 2;
      }
      directories.emplace_back(argv[++i]);
      continue;
    }
    if (a == "-D" || a == "--maxdepth") {
      if (i + 1 >= argc) {
        std::cerr << "missing argument for " << a << '\n';
        return 2;
      }
      max_depth = std::atoi(argv[++i]);
      continue;
    }
    if (a.starts_with("--maxdepth=")) {
      max_depth = std::atoi(std::string(a.substr(11)).c_str());
      continue;
    }
    if (a.starts_with('-')) {
      std::cerr << "unknown option: " << a << '\n';
      return 2;
    }
    if (search_mode) {
      query_words.emplace_back(argv[i]);
    } else {
      directories.emplace_back(argv[i]);
    }
  }

  if (search_mode && filter_expr.empty() && !query_words.empty()) {
    filter_expr = query_words.front();
    for (std::size_t i = 1; i < query_words.size(); ++i) {
      filter_expr.push_back(' ');
      filter_expr += query_words[i];
    }
  }

  if (directories.empty()) {
    directories.emplace_back(".");
  }

  dirtoo::filter::MatchFuncPtr match;
  if (filter_expr.empty()) {
    match = std::make_shared<dirtoo::filter::AlwaysTrue>();
  } else {
    const auto parsed = dirtoo::filter::parse_filter(filter_expr);
    if (!parsed) {
      std::cerr << "parse error at " << parsed.error().position << ": "
                << parsed.error().message << '\n';
      return 1;
    }
    match = *parsed;
  }

  dirtoo::filter::SearchOptions opts;
  opts.max_depth = max_depth;
  opts.show_hidden = show_hidden;

  std::uint64_t total_errors = 0;
  for (const auto& directory : directories) {
    const auto stats = dirtoo::filter::search_directory(
        fs::path{directory}, *match, opts,
        [&](const dirtoo::filter::FilterItem& item) {
          if (list_mode) {
            std::cout << (item.is_directory ? 'd' : '-') << ' ' << item.size << ' ';
          }
          std::cout << item.path.string();
          if (null_sep) {
            std::cout << '\0';
          } else {
            std::cout << '\n';
          }
        });
    total_errors += stats.errors;
  }

  if (total_errors > 0) {
    std::cerr << "warning: " << total_errors << " path(s) could not be read\n";
  }
  return 0;
}

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Remove empty directories (Python programs/rmdir.py).
/// Without -r: std::filesystem::remove on a single empty directory.
/// With -r: depth-first walk, removing only empty directories (never deletes files).

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace fs = std::filesystem;

namespace {

const char* kName = "dt-rmdir";

void usage()
{
  std::cerr << "usage: " << kName << " [-v|--verbose] [-r|--recursive] <directory> [directory…]\n"
               "  Remove empty directories. With --recursive, remove empty directory trees\n"
               "  bottom-up (files inside a directory cause failure for that directory).\n";
}

bool remove_empty_directory(const fs::path& directory, bool verbose)
{
  if (verbose) {
    std::cout << kName << ": removing directory, '" << directory.string() << "'\n";
  }
  std::error_code ec;
  if (!fs::is_directory(directory, ec) || ec) {
    std::cerr << kName << ": failed to remove '" << directory.string()
              << "': not a directory\n";
    return false;
  }
  // remove() only succeeds for empty directories (and non-directory files); we
  // already required is_directory, so non-empty dirs fail with directory_not_empty.
  if (!fs::remove(directory, ec) || ec) {
    std::cerr << kName << ": failed to remove '" << directory.string() << "': "
              << (ec ? ec.message() : "not empty or not removable") << '\n';
    return false;
  }
  return true;
}

bool remove_directory_recursive(const fs::path& directory, bool verbose)
{
  std::error_code ec;
  if (!fs::is_directory(directory, ec) || ec) {
    std::cerr << kName << ": failed to remove '" << directory.string()
              << "': not a directory\n";
    return false;
  }

  // Collect paths depth-first (children before parents), like os.walk(..., topdown=False).
  std::vector<fs::path> dirs;
  for (auto it = fs::recursive_directory_iterator(
           directory, fs::directory_options::skip_permission_denied, ec);
       !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
    std::error_code st_ec;
    if (it->is_directory(st_ec) && !st_ec) {
      dirs.push_back(it->path());
    }
  }
  // Children of `directory` first; then `directory` itself.
  // recursive_directory_iterator yields parents before children in default
  // order — reverse so we rmdir leaves first.
  bool ok = true;
  for (auto it = dirs.rbegin(); it != dirs.rend(); ++it) {
    if (!remove_empty_directory(*it, verbose)) {
      ok = false;
    }
  }
  if (!remove_empty_directory(directory, verbose)) {
    ok = false;
  }
  return ok;
}

} // namespace

int main(int argc, char* argv[])
{
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--version" || std::string_view(argv[i]) == "-V") {
      std::cout << "dirtoo " DIRTOO_VERSION "\n";
      return 0;
    }
  }

  bool verbose = false;
  bool recursive = false;
  int argi = 1;
  while (argi < argc && std::string_view(argv[argi]).starts_with('-')) {
    const std::string_view a{argv[argi]};
    if (a == "-v" || a == "--verbose") {
      verbose = true;
    } else if (a == "-r" || a == "--recursive") {
      recursive = true;
    } else if (a == "--help" || a == "-h") {
      usage();
      return 0;
    } else if (a == "--") {
      ++argi;
      break;
    } else {
      std::cerr << kName << ": unknown option: " << a << '\n';
      usage();
      return 2;
    }
    ++argi;
  }

  if (argi >= argc) {
    usage();
    return 2;
  }

  int failures = 0;
  for (; argi < argc; ++argi) {
    const fs::path dir{argv[argi]};
    const bool ok = recursive ? remove_directory_recursive(dir, verbose)
                              : remove_empty_directory(dir, verbose);
    if (!ok) {
      ++failures;
    }
  }
  return failures == 0 ? 0 : 1;
}

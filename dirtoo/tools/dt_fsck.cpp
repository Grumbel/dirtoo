// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Check pathnames for invalid UTF-8, newlines, and extreme length (Python dt-fsck).

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace fs = std::filesystem;

namespace {

bool is_valid_utf8(std::string_view s)
{
  const auto* p = reinterpret_cast<const unsigned char*>(s.data());
  const auto* end = p + s.size();
  while (p < end) {
    if (*p <= 0x7F) {
      ++p;
      continue;
    }
    int need = 0;
    if ((*p & 0xE0) == 0xC0) {
      need = 1;
      if ((*p & 0xFE) == 0xC0) {
        return false; // overlong
      }
    } else if ((*p & 0xF0) == 0xE0) {
      need = 2;
    } else if ((*p & 0xF8) == 0xF0) {
      need = 3;
      if (*p > 0xF4) {
        return false;
      }
    } else {
      return false;
    }
    ++p;
    for (int i = 0; i < need; ++i) {
      if (p >= end || (*p & 0xC0) != 0x80) {
        return false;
      }
      ++p;
    }
  }
  return true;
}

void fail(std::string_view kind, const std::string& path)
{
  std::cout << kind << " FAIL " << path << '\n';
}

void good(const std::string& path, bool verbose)
{
  if (verbose) {
    std::cout << "GOOD " << path << '\n';
  }
}

void check_path(const std::string& path, bool verbose)
{
  if (path.size() > 200) {
    fail("LENGTH", path);
  }
  if (path.find('\n') != std::string::npos) {
    fail("NEWLINE", path);
  }
  if (!is_valid_utf8(path)) {
    fail("UTF-8", path);
  } else {
    good(path, verbose);
  }
}

void check_tree(const fs::path& root, bool recursive, bool verbose)
{
  std::error_code ec;
  if (recursive && fs::is_directory(root, ec)) {
    std::uint64_t dir_count = 0;
    std::uint64_t file_count = 0;
    for (fs::directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
      if (ec) {
        break;
      }
      const auto st = it->status(ec);
      if (ec) {
        continue;
      }
      if (fs::is_directory(st)) {
        ++dir_count;
      } else {
        ++file_count;
      }
    }
    if (dir_count > 1000) {
      fail("MANY DIRS", root.string());
    }
    if (file_count > 1000) {
      fail("MANY FILES", root.string());
    }
    for (fs::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
      if (ec) {
        ec.clear();
        continue;
      }
      check_path(it->path().string(), verbose);
    }
  } else {
    check_path(root.string(), verbose);
  }
}

void usage(const char* argv0)
{
  std::cerr << "Usage: " << argv0 << " [options] <path>…\n"
               "  Check pathnames for invalid UTF-8, embedded newlines, and extreme length.\n\n"
               "Options:\n"
               "  -r, --recursive   Walk directories\n"
               "  -v, --verbose     Print GOOD lines for valid paths\n"
               "  -V, --version\n"
               "  -h, --help\n";
}

} // namespace

int main(int argc, char** argv)
{
  bool recursive = false;
  bool verbose = false;
  std::vector<std::string> paths;

  for (int i = 1; i < argc; ++i) {
    const std::string_view a{argv[i]};
    if (a == "-V" || a == "--version") {
      std::cout << "dirtoo " DIRTOO_VERSION "\n";
      return 0;
    }
    if (a == "-h" || a == "--help") {
      usage(argv[0]);
      return 0;
    }
    if (a == "-r" || a == "--recursive") {
      recursive = true;
      continue;
    }
    if (a == "-v" || a == "--verbose") {
      verbose = true;
      continue;
    }
    if (a.starts_with('-')) {
      std::cerr << "unknown option: " << a << '\n';
      return 2;
    }
    paths.emplace_back(argv[i]);
  }

  if (paths.empty()) {
    usage(argv[0]);
    return 2;
  }

  for (const auto& p : paths) {
    check_tree(p, recursive, verbose);
  }
  return 0;
}

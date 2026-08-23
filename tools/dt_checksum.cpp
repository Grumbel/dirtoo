// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Multi-algo file checksums with SQLite cache (like md5sum/sha256sum + cache).

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/hash/hash_file.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace fs = std::filesystem;
namespace dh = dirtoo::hash;

namespace {

enum class Algo { Sha256, Md5, Sha1, Crc32, All };

void usage(const char* argv0)
{
  std::cerr
      << "Usage: " << argv0 << " [options] <path>…\n\n"
      << "Compute or recall file checksums (CRC32, MD5, SHA-1, SHA-256) in one pass.\n"
      << "Results are cached under $XDG_CACHE_HOME/dirtoo/checksums.sqlite and reused\n"
      << "while size and mtime match.\n\n"
      << "Options:\n"
      << "  -a, --algo ALG     md5|sha1|sha256|crc32|all  (default: sha256)\n"
      << "  --refresh          Ignore cache; re-read files\n"
      << "  --cached-only      Never read file content; fail if not cached\n"
      << "  --no-cache         Hash but do not read/write the cache\n"
      << "  --db PATH          Checksum database path\n"
      << "  -V, --version\n"
      << "  -h, --help\n";
}

const std::string& pick(const dh::FileDigests& d, Algo a)
{
  switch (a) {
  case Algo::Md5:
    return d.md5_hex;
  case Algo::Sha1:
    return d.sha1_hex;
  case Algo::Crc32:
    return d.crc32_hex;
  case Algo::Sha256:
  default:
    return d.sha256_hex;
  }
}

std::optional<Algo> parse_algo(std::string_view s)
{
  if (s == "md5") {
    return Algo::Md5;
  }
  if (s == "sha1") {
    return Algo::Sha1;
  }
  if (s == "sha256") {
    return Algo::Sha256;
  }
  if (s == "crc32") {
    return Algo::Crc32;
  }
  if (s == "all") {
    return Algo::All;
  }
  return std::nullopt;
}

void print_digests(const dh::FileDigests& d, Algo algo, const std::string& path_display)
{
  if (algo == Algo::All) {
    std::cout << "CRC32  " << d.crc32_hex << "  " << path_display << '\n';
    std::cout << "MD5    " << d.md5_hex << "  " << path_display << '\n';
    std::cout << "SHA1   " << d.sha1_hex << "  " << path_display << '\n';
    std::cout << "SHA256 " << d.sha256_hex << "  " << path_display << '\n';
    return;
  }
  // coreutils-style: <hex>  <path>
  std::cout << pick(d, algo) << "  " << path_display << '\n';
}

} // namespace

int main(int argc, char** argv)
{
  Algo algo = Algo::Sha256;
  bool refresh = false;
  bool cached_only = false;
  bool no_cache = false;
  std::string db_path;
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
    if (a == "--refresh") {
      refresh = true;
      continue;
    }
    if (a == "--cached-only") {
      cached_only = true;
      continue;
    }
    if (a == "--no-cache") {
      no_cache = true;
      continue;
    }
    if (a == "-a" || a == "--algo") {
      if (i + 1 >= argc) {
        std::cerr << "missing argument for " << a << '\n';
        return 2;
      }
      const auto parsed = parse_algo(argv[++i]);
      if (!parsed) {
        std::cerr << "unknown algo (use md5|sha1|sha256|crc32|all)\n";
        return 2;
      }
      algo = *parsed;
      continue;
    }
    if (a.starts_with("--algo=")) {
      const auto parsed = parse_algo(a.substr(7));
      if (!parsed) {
        std::cerr << "unknown algo\n";
        return 2;
      }
      algo = *parsed;
      continue;
    }
    if (a == "--db") {
      if (i + 1 >= argc) {
        std::cerr << "missing argument for --db\n";
        return 2;
      }
      db_path = argv[++i];
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
  if (cached_only && refresh) {
    std::cerr << "--cached-only and --refresh are mutually exclusive\n";
    return 2;
  }

  dh::ChecksumStore store;
  if (!no_cache) {
    std::string err;
    const fs::path db =
        db_path.empty() ? dh::ChecksumStore::default_path() : fs::path{db_path};
    if (!store.open(db, &err)) {
      std::cerr << "checksum cache open failed: " << err << '\n';
      return 1;
    }
  }

  int exit_code = 0;
  for (const auto& pstr : paths) {
    std::error_code ec;
    const fs::path path = fs::absolute(pstr, ec);
    if (ec) {
      std::cerr << pstr << ": " << ec.message() << '\n';
      exit_code = 1;
      continue;
    }
    const std::string key = path.lexically_normal().string();

    std::optional<dh::FileDigests> digests;
    if (cached_only) {
      digests = store.get(key);
      if (!digests) {
        std::cerr << key << ": not in checksum cache\n";
        exit_code = 1;
        continue;
      }
    } else if (no_cache) {
      dh::HashError herr;
      digests = dh::hash_file(path, {}, &herr);
      if (!digests) {
        std::cerr << key << ": " << herr.message << '\n';
        exit_code = 1;
        continue;
      }
    } else {
      dh::HashError herr;
      digests = store.ensure(path, key, refresh, &herr);
      if (!digests) {
        std::cerr << key << ": " << herr.message << '\n';
        exit_code = 1;
        continue;
      }
    }
    print_digests(*digests, algo, key);
  }
  return exit_code;
}

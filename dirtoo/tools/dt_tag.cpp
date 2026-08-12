// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

/// Tag files by content identity (SHA-256 from checksum cache). Does not hash.

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/tags/tag_store.hpp"

#include <filesystem>
#include <optional>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace fs = std::filesystem;

namespace {

void usage(const char* argv0)
{
  std::cerr
      << "Usage: " << argv0 << " <command> [options] [args…]\n\n"
      << "Tag files using SHA-256 from the checksum cache (never hashes itself).\n"
      << "Run dt-checksum on files first, or pass --hash-if-needed.\n"
      << "Tags are indirected by integer id; rename only updates the definition.\n\n"
      << "Commands:\n"
      << "  add    <tag> <path>…     Attach tag (checksum must exist unless --hash-if-needed)\n"
      << "  remove <tag> <path>…     Remove tag from files\n"
      << "  list   [path]            List tags on path, or all tag definitions\n"
      << "  files  <tag>             List known paths / sha256 for tag\n"
      << "  def    <tag> [--label L] [--color #rgb] [--badge ID]\n"
      << "  rename <old> <new>       Rename tag definition (file links keep tag id)\n\n"
      << "Options:\n"
      << "  --hash-if-needed   Call checksum ensure() once if cache miss (add only)\n"
      << "  --tags-db PATH     Tags database (default: $XDG_DATA_HOME/dirtoo/tags.sqlite)\n"
      << "  --checksum-db PATH Checksum database\n"
      << "  -V, --version\n"
      << "  -h, --help\n";
}

std::string path_key(const std::string& p)
{
  std::error_code ec;
  const fs::path abs = fs::absolute(p, ec);
  if (ec) {
    return p;
  }
  return abs.lexically_normal().string();
}

} // namespace

int main(int argc, char** argv)
{
  if (argc < 2) {
    usage(argv[0]);
    return 2;
  }

  std::string tags_db;
  std::string checksum_db;
  bool hash_if_needed = false;
  std::vector<std::string> args;

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
    if (a == "--hash-if-needed") {
      hash_if_needed = true;
      continue;
    }
    if (a == "--tags-db") {
      if (i + 1 >= argc) {
        return 2;
      }
      tags_db = argv[++i];
      continue;
    }
    if (a == "--checksum-db") {
      if (i + 1 >= argc) {
        return 2;
      }
      checksum_db = argv[++i];
      continue;
    }
    args.emplace_back(argv[i]);
  }

  if (args.empty()) {
    usage(argv[0]);
    return 2;
  }

  const std::string cmd = args[0];
  args.erase(args.begin());

  dirtoo::tags::TagStore tags;
  std::string err;
  const fs::path tdb =
      tags_db.empty() ? dirtoo::tags::TagStore::default_path() : fs::path{tags_db};
  if (!tags.open(tdb, &err)) {
    std::cerr << "tags db: " << err << '\n';
    return 1;
  }

  dirtoo::hash::ChecksumStore checksums;
  const fs::path cdb = checksum_db.empty() ? dirtoo::hash::ChecksumStore::default_path()
                                           : fs::path{checksum_db};
  if (!checksums.open(cdb, &err)) {
    std::cerr << "checksum db: " << err << '\n';
    return 1;
  }

  auto resolve = [&](const std::string& p) -> std::optional<std::int64_t> {
    const std::string key = path_key(p);
    std::string e;
    if (auto id = tags.resolve_path(checksums, key, &e)) {
      return id;
    }
    if (hash_if_needed) {
      dirtoo::hash::HashError herr;
      if (checksums.ensure(fs::path{key}, key, false, &herr)) {
        e.clear();
        return tags.resolve_path(checksums, key, &e);
      }
      std::cerr << key << ": " << herr.message << '\n';
      return std::nullopt;
    }
    std::cerr << key << ": " << e << '\n';
    return std::nullopt;
  };

  if (cmd == "add") {
    if (args.size() < 2) {
      usage(argv[0]);
      return 2;
    }
    const std::string tag = args[0];
    int rc = 0;
    for (std::size_t i = 1; i < args.size(); ++i) {
      auto id = resolve(args[i]);
      if (!id) {
        rc = 1;
        continue;
      }
      std::string e;
      if (!tags.add_tag_to_file(*id, tag, &e)) {
        std::cerr << args[i] << ": " << e << '\n';
        rc = 1;
      }
    }
    return rc;
  }

  if (cmd == "remove") {
    if (args.size() < 2) {
      usage(argv[0]);
      return 2;
    }
    const std::string tag = args[0];
    int rc = 0;
    for (std::size_t i = 1; i < args.size(); ++i) {
      auto id = resolve(args[i]);
      if (!id) {
        rc = 1;
        continue;
      }
      std::string e;
      if (!tags.remove_tag_from_file(*id, tag, &e)) {
        std::cerr << args[i] << ": " << e << '\n';
        rc = 1;
      }
    }
    return rc;
  }

  if (cmd == "list") {
    if (args.empty()) {
      for (const auto& def : tags.list_tags()) {
        std::cout << def.name << "  id=" << def.id;
        if (!def.label.empty() && def.label != def.name) {
          std::cout << "  (" << def.label << ")";
        }
        if (!def.color.empty()) {
          std::cout << "  color=" << def.color;
        }
        if (!def.badge.empty()) {
          std::cout << "  badge=" << def.badge;
        }
        std::cout << '\n';
      }
      return 0;
    }
    int rc = 0;
    for (const auto& p : args) {
      const std::string key = path_key(p);
      auto digests = checksums.get(key);
      if (!digests) {
        std::cerr << key << ": checksum unknown; run dt-checksum first\n";
        rc = 1;
        continue;
      }
      auto file_tags = tags.tags_for_sha256(digests->sha256_hex);
      std::cout << key << "  sha256=" << digests->sha256_hex << '\n';
      if (file_tags.empty()) {
        std::cout << "  (no tags)\n";
      } else {
        for (const auto& t : file_tags) {
          std::cout << "  " << t << '\n';
        }
      }
    }
    return rc;
  }

  if (cmd == "files") {
    if (args.size() != 1) {
      usage(argv[0]);
      return 2;
    }
    for (const auto& tf : tags.files_for_tag(args[0])) {
      std::cout << tf.sha256;
      if (tf.paths.empty()) {
        std::cout << "  (no path aliases)\n";
      } else {
        std::cout << '\n';
        for (const auto& p : tf.paths) {
          std::cout << "  " << p << '\n';
        }
      }
    }
    return 0;
  }

  if (cmd == "def") {
    if (args.empty()) {
      usage(argv[0]);
      return 2;
    }
    const std::string tag = args[0];
    std::optional<std::string> label, color, badge;
    for (std::size_t i = 1; i < args.size(); ++i) {
      if (args[i] == "--label" && i + 1 < args.size()) {
        label = args[++i];
      } else if (args[i] == "--color" && i + 1 < args.size()) {
        color = args[++i];
      } else if (args[i] == "--badge" && i + 1 < args.size()) {
        badge = args[++i];
      } else {
        std::cerr << "unknown def option: " << args[i] << '\n';
        return 2;
      }
    }
    std::string e;
    if (!tags.set_tag_meta(tag, label, color, badge, &e)) {
      std::cerr << e << '\n';
      return 1;
    }
    if (auto def = tags.get_tag(tag)) {
      std::cout << def->name << " label=" << def->label << " color=" << def->color
                << " badge=" << def->badge << '\n';
    }
    return 0;
  }

  if (cmd == "rename") {
    if (args.size() != 2) {
      usage(argv[0]);
      return 2;
    }
    std::string e;
    if (!tags.rename_tag(args[0], args[1], &e)) {
      std::cerr << e << '\n';
      return 1;
    }
    if (auto def = tags.get_tag(args[1])) {
      std::cout << "renamed -> " << def->name << " (id=" << def->id << ")\n";
    }
    return 0;
  }

  std::cerr << "unknown command: " << cmd << '\n';
  usage(argv[0]);
  return 2;
}

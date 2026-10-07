// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "archive_listing.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using dirtoo::app::ArchiveListing;

namespace {

void add_file(archive* a, const std::string& name, const std::string& data)
{
  archive_entry* e = archive_entry_new();
  archive_entry_set_pathname(e, name.c_str());
  archive_entry_set_filetype(e, AE_IFREG);
  archive_entry_set_perm(e, 0644);
  archive_entry_set_size(e, static_cast<la_int64_t>(data.size()));
  archive_write_header(a, e);
  archive_write_data(a, data.data(), data.size());
  archive_entry_free(e);
}

void add_dir(archive* a, const std::string& name)
{
  archive_entry* e = archive_entry_new();
  archive_entry_set_pathname(e, name.c_str());
  archive_entry_set_filetype(e, AE_IFDIR);
  archive_entry_set_perm(e, 0755);
  archive_write_header(a, e);
  archive_entry_free(e);
}

fs::path make_archive(const char* tag)
{
  const auto dir = fs::temp_directory_path()
                   / (std::string("dirtoo-listing-") + tag + "-" + std::to_string(::getpid()));
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir);
  const auto tar = dir / "a.tar";
  archive* a = archive_write_new();
  archive_write_set_format_pax(a);
  archive_write_open_filename(a, tar.c_str());
  add_file(a, "x.txt", "x");
  add_dir(a, "top/");
  add_file(a, "top/a.txt", "a");
  add_file(a, "top/sub/b.txt", "b");
  add_file(a, "top/sub/c.txt", "c");
  add_file(a, "top/sub/deep/d.txt", "d");
  add_dir(a, "top/empty/");
  archive_write_close(a);
  archive_write_free(a);
  return tar;
}

} // namespace

TEST_CASE("ArchiveListing child counts per directory row", "[archive][listing]")
{
  const auto tar = make_archive("counts");
  ArchiveListing listing;
  std::string err;
  REQUIRE(listing.load(tar, &err));

  // Archive root: only "top" is a directory row. Distinct direct children of
  // top: a.txt, sub, empty.
  auto root_counts = listing.child_counts_for(dirtoo::fs::Location::from_archive(tar, ""));
  REQUIRE(root_counts.size() == 1);
  CHECK(root_counts.begin()->second == 3);

  // One level down: sub has b.txt, c.txt, deep; empty has nothing.
  auto top_counts = listing.child_counts_for(dirtoo::fs::Location::from_archive(tar, "top"));
  std::int64_t sub = -1;
  std::int64_t empty = -1;
  for (const auto& [path, n] : top_counts) {
    const auto name = fs::path(path).filename().string();
    if (name == "sub") {
      sub = n;
    } else if (name == "empty") {
      empty = n;
    }
  }
  CHECK(top_counts.size() == 2);
  CHECK(sub == 3);
  CHECK(empty == 0);
  fs::remove_all(tar.parent_path());
}

TEST_CASE("ArchiveListing stamp detects a replaced archive", "[archive][listing]")
{
  const auto tar = make_archive("stamp");
  ArchiveListing listing;
  REQUIRE(listing.load(tar, nullptr));
  const auto stamp = listing.stamp();
  CHECK(ArchiveListing::stamp_matches(tar, stamp));

  // Append a byte: the size changes.
  {
    std::ofstream out(tar, std::ios::app | std::ios::binary);
    out << '\0';
  }
  CHECK_FALSE(ArchiveListing::stamp_matches(tar, stamp));
  CHECK_FALSE(ArchiveListing::stamp_matches(tar.parent_path() / "missing.tar", stamp));
  fs::remove_all(tar.parent_path());
}

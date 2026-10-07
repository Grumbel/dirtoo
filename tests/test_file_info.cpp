// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/fs/file_info.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fcntl.h>
#include <sys/stat.h>

#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace {

struct TempDir {
  std::filesystem::path path;
  TempDir()
  {
    path = std::filesystem::temp_directory_path()
           / ("dirtoo-test-fileinfo-" + std::to_string(::getpid()));
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
  }
  ~TempDir()
  {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

} // namespace

TEST_CASE("FileInfo captures symlink target metadata", "[fileinfo]")
{
  TempDir tmp;
  const auto dir = tmp.path / "dir";
  const auto file = tmp.path / "file.txt";
  std::filesystem::create_directory(dir);
  std::ofstream(file) << "x";
  std::filesystem::create_directory_symlink("dir", tmp.path / "link-dir");
  std::filesystem::create_symlink(file, tmp.path / "link-file");
  std::filesystem::create_symlink("missing", tmp.path / "link-broken");

  SECTION("plain entries have no link data")
  {
    const auto fi = dirtoo::fs::FileInfo::from_path(file);
    REQUIRE_FALSE(fi.is_symlink());
    REQUIRE(fi.symlink_target().empty());
    REQUIRE_FALSE(fi.is_broken_symlink());
    REQUIRE_FALSE(fi.symlink_target_is_directory());
  }

  SECTION("link to directory (relative target)")
  {
    const auto fi = dirtoo::fs::FileInfo::from_path(tmp.path / "link-dir");
    REQUIRE(fi.is_symlink());
    REQUIRE_FALSE(fi.is_directory());
    REQUIRE(fi.symlink_target() == "dir");
    REQUIRE_FALSE(fi.is_broken_symlink());
    REQUIRE(fi.symlink_target_is_directory());
  }

  SECTION("link to file via directory_entry")
  {
    const auto fi =
        dirtoo::fs::FileInfo::from_directory_entry(std::filesystem::directory_entry(tmp.path / "link-file"));
    REQUIRE(fi.is_symlink());
    REQUIRE(fi.symlink_target() == file);
    REQUIRE_FALSE(fi.is_broken_symlink());
    REQUIRE_FALSE(fi.symlink_target_is_directory());
  }

  SECTION("dangling link")
  {
    const auto fi = dirtoo::fs::FileInfo::from_path(tmp.path / "link-broken");
    REQUIRE(fi.is_symlink());
    REQUIRE(fi.symlink_target() == "missing");
    REQUIRE(fi.is_broken_symlink());
    REQUIRE_FALSE(fi.symlink_target_is_directory());
  }
}

TEST_CASE("FileInfo keeps nanosecond mtimes and real zero times", "[file_info][mtime]")
{
  namespace fs = std::filesystem;
  const auto dir = fs::temp_directory_path() / "dirtoo-test-fileinfo-mtime";
  fs::remove_all(dir);
  fs::create_directories(dir);

  auto make = [&](const char* name, timespec ts) {
    const auto p = dir / name;
    { std::ofstream(p) << "x"; }
    timespec times[2] = {ts, ts};
    REQUIRE(::utimensat(AT_FDCWD, p.c_str(), times, 0) == 0);
    return dirtoo::fs::FileInfo::from_path(p);
  };

  // Sub-second precision survives (files created in one second stay ordered).
  const auto a = make("a", {1577934245, 100000000});
  const auto b = make("b", {1577934245, 900000000});
  REQUIRE(a.has_mtime());
  CHECK(a.mtime_unix_ns() == 1577934245LL * 1'000'000'000LL + 100000000LL);
  CHECK(a.mtime() < b.mtime());

  // 0 (SOURCE_DATE_EPOCH=0 tarballs) and 1 (what Nix stamps on /nix/store)
  // are times, not "unknown".
  const auto zero = make("zero", {0, 0});
  CHECK(zero.has_mtime());
  CHECK(zero.mtime_unix_ns() == 0);
  const auto one = make("one", {1, 0});
  CHECK(one.mtime_unix_ns() == 1'000'000'000LL);
  const auto before = make("before1970", {-86400, 0});
  CHECK(before.mtime_unix_ns() == -86400LL * 1'000'000'000LL);

  // Real Nix store path, if this machine has one.
  std::error_code ec;
  if (fs::is_directory("/nix/store", ec)) {
    for (const auto& e : fs::directory_iterator("/nix/store", ec)) {
      if (e.path().filename().string().starts_with('.')) {
        continue;  // .links / .lock are Nix bookkeeping with real times
      }
      const auto info = dirtoo::fs::FileInfo::from_path(e.path());
      CHECK(info.has_mtime());
      CHECK(info.mtime_unix_ns() == 1'000'000'000LL);  // canonicalised to epoch + 1 s
      break;
    }
  }

  // Entries that never had a time say so (they used to read as the year 2174).
  const auto member = dirtoo::fs::FileInfo::synthetic(
      dirtoo::fs::Location::from_archive("/tmp/a.zip", "x"), "x", false, 1);
  CHECK_FALSE(member.has_mtime());
  CHECK_FALSE(member.mtime_unix_ns().has_value());
  fs::remove_all(dir);
}

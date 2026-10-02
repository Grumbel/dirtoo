// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/fs/file_info.hpp"

#include <catch2/catch_test_macros.hpp>

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

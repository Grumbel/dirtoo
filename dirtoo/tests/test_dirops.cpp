// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/ops.hpp"
#include "dirops/util.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

fs::path make_temp_dir(const char* name)
{
  const auto dir = fs::temp_directory_path() / name;
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

void write_file(const fs::path& p, const std::string& content)
{
  std::ofstream out(p);
  out << content;
}

std::string read_file(const fs::path& p)
{
  std::ifstream in(p);
  return std::string(std::istreambuf_iterator<char>(in), {});
}

} // namespace

TEST_CASE("unique_path avoids existing names", "[dirops][util]")
{
  const auto dir = make_temp_dir("dirtoo-unique");
  const auto base = dir / "file.txt";
  write_file(base, "a");
  const auto u = dirops::unique_path(base);
  REQUIRE(u != base);
  REQUIRE_FALSE(fs::exists(u));
  REQUIRE(u.filename().string().find("file") != std::string::npos);
  fs::remove_all(dir);
}

TEST_CASE("rename_path moves a file", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-rename");
  const auto from = dir / "a.txt";
  const auto to = dir / "b.txt";
  write_file(from, "hello");

  auto result = dirops::rename_path(from, to);
  REQUIRE(result.has_value());
  REQUIRE(fs::exists(to));
  REQUIRE_FALSE(fs::exists(from));
  REQUIRE(read_file(to) == "hello");

  fs::remove_all(dir);
}

TEST_CASE("copy_path copies a regular file", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-copy");
  const auto from = dir / "src.txt";
  const auto to = dir / "dst.txt";
  write_file(from, "payload");

  auto result = dirops::copy_path(from, to);
  REQUIRE(result.has_value());
  REQUIRE(fs::exists(from));
  REQUIRE(fs::exists(to));
  REQUIRE(read_file(to) == "payload");

  fs::remove_all(dir);
}

TEST_CASE("copy_path into directory uses basename", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-copy-into");
  const auto sub = dir / "sub";
  fs::create_directory(sub);
  const auto from = dir / "src.txt";
  write_file(from, "x");

  auto result = dirops::copy_path(from, sub);
  REQUIRE(result.has_value());
  REQUIRE(fs::exists(sub / "src.txt"));
  REQUIRE(read_file(sub / "src.txt") == "x");

  fs::remove_all(dir);
}

TEST_CASE("copy_path recursive directory", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-copy-tree");
  const auto src = dir / "src";
  const auto dst = dir / "dst";
  fs::create_directories(src / "nested");
  write_file(src / "a.txt", "a");
  write_file(src / "nested" / "b.txt", "b");

  auto result = dirops::copy_path(src, dst);
  REQUIRE(result.has_value());
  REQUIRE(fs::exists(dst / "a.txt"));
  REQUIRE(fs::exists(dst / "nested" / "b.txt"));
  REQUIRE(read_file(dst / "nested" / "b.txt") == "b");
  // source still present
  REQUIRE(fs::exists(src / "a.txt"));

  fs::remove_all(dir);
}

TEST_CASE("copy_path conflict Fail", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-conflict-fail");
  write_file(dir / "a.txt", "1");
  write_file(dir / "b.txt", "2");

  dirops::Options opt;
  opt.conflict = dirops::ConflictPolicy::Fail;
  auto result = dirops::copy_path(dir / "a.txt", dir / "b.txt", opt);
  REQUIRE_FALSE(result.has_value());

  fs::remove_all(dir);
}

TEST_CASE("copy_path conflict Overwrite", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-conflict-ow");
  write_file(dir / "a.txt", "new");
  write_file(dir / "b.txt", "old");

  dirops::Options opt;
  opt.conflict = dirops::ConflictPolicy::Overwrite;
  auto result = dirops::copy_path(dir / "a.txt", dir / "b.txt", opt);
  REQUIRE(result.has_value());
  REQUIRE(read_file(dir / "b.txt") == "new");

  fs::remove_all(dir);
}

TEST_CASE("copy_path conflict Rename", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-conflict-ren");
  write_file(dir / "a.txt", "new");
  write_file(dir / "b.txt", "old");

  dirops::Options opt;
  opt.conflict = dirops::ConflictPolicy::Rename;
  auto result = dirops::copy_path(dir / "a.txt", dir / "b.txt", opt);
  REQUIRE(result.has_value());
  REQUIRE(read_file(dir / "b.txt") == "old");
  REQUIRE(result->items.size() == 1);
  REQUIRE(result->items[0].destination != dir / "b.txt");
  REQUIRE(fs::exists(result->items[0].destination));

  fs::remove_all(dir);
}

TEST_CASE("copy_path conflict Skip", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-conflict-skip");
  write_file(dir / "a.txt", "new");
  write_file(dir / "b.txt", "old");

  dirops::Options opt;
  opt.conflict = dirops::ConflictPolicy::Skip;
  auto result = dirops::copy_path(dir / "a.txt", dir / "b.txt", opt);
  REQUIRE(result.has_value());
  REQUIRE(result->items[0].skipped);
  REQUIRE(read_file(dir / "b.txt") == "old");

  fs::remove_all(dir);
}

TEST_CASE("move_path same directory", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-move");
  write_file(dir / "a.txt", "m");
  auto result = dirops::move_path(dir / "a.txt", dir / "b.txt");
  REQUIRE(result.has_value());
  REQUIRE_FALSE(fs::exists(dir / "a.txt"));
  REQUIRE(read_file(dir / "b.txt") == "m");
  fs::remove_all(dir);
}

TEST_CASE("swap_names exchanges two files", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-swap");
  write_file(dir / "a.txt", "A");
  write_file(dir / "b.txt", "B");

  auto result = dirops::swap_names(dir / "a.txt", dir / "b.txt");
  REQUIRE(result.has_value());
  REQUIRE(read_file(dir / "a.txt") == "B");
  REQUIRE(read_file(dir / "b.txt") == "A");

  fs::remove_all(dir);
}

TEST_CASE("create_directory and remove_path", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-mkdir");
  const auto sub = dir / "newdir";
  auto created = dirops::create_directory(sub);
  REQUIRE(created.has_value());
  REQUIRE(fs::is_directory(sub));

  auto removed = dirops::remove_path(sub);
  REQUIRE(removed.has_value());
  REQUIRE_FALSE(fs::exists(sub));

  fs::remove_all(dir);
}

TEST_CASE("dry_run does not touch filesystem", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-dry");
  write_file(dir / "a.txt", "x");

  dirops::Options opt;
  opt.dry_run = true;
  auto result = dirops::copy_path(dir / "a.txt", dir / "b.txt", opt);
  REQUIRE(result.has_value());
  REQUIRE_FALSE(fs::exists(dir / "b.txt"));

  fs::remove_all(dir);
}

TEST_CASE("create_file makes empty file", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-create-file");
  const auto path = dir / "empty.txt";
  auto result = dirops::create_file(path);
  REQUIRE(result.has_value());
  REQUIRE(fs::exists(path));
  REQUIRE(fs::file_size(path) == 0);
  // second create fails
  auto again = dirops::create_file(path);
  REQUIRE_FALSE(again.has_value());
  fs::remove_all(dir);
}

TEST_CASE("create_symlink makes link", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-symlink");
  const auto target = dir / "target.txt";
  write_file(target, "data");
  const auto link = dir / "link.txt";
  auto result = dirops::create_symlink(target, link);
  REQUIRE(result.has_value());
  REQUIRE(fs::is_symlink(link));
  fs::remove_all(dir);
}

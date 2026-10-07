// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/ops.hpp"
#include "dirops/util.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

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

TEST_CASE("Overwrite refuses directory destination", "[dirops]")
{
  // copy/move place the source basename *into* an existing directory destination.
  // Safety is exercised when that resolved path is itself a directory (would
  // require deleting a tree to replace it with a file) — never remove_all.
  const auto dir = make_temp_dir("dirtoo-test-conflict-ow-dir");
  write_file(dir / "a.txt", "new");
  // target/a.txt is a directory (same final path as copy/move into target).
  fs::create_directories(dir / "target" / "a.txt");
  write_file(dir / "target" / "a.txt" / "keep.txt", "important");

  dirops::Options opt;
  opt.conflict = dirops::ConflictPolicy::Overwrite;
  auto result = dirops::copy_path(dir / "a.txt", dir / "target", opt);
  REQUIRE_FALSE(result.has_value());
  REQUIRE(fs::exists(dir / "target" / "a.txt" / "keep.txt"));
  REQUIRE(read_file(dir / "target" / "a.txt" / "keep.txt") == "important");

  auto moved = dirops::move_path(dir / "a.txt", dir / "target", opt);
  REQUIRE_FALSE(moved.has_value());
  REQUIRE(fs::exists(dir / "target" / "a.txt" / "keep.txt"));
  REQUIRE(fs::exists(dir / "a.txt"));

  fs::remove_all(dir);
}

TEST_CASE("rename_path Overwrite refuses directory", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-rename-ow-dir");
  write_file(dir / "a.txt", "x");
  fs::create_directory(dir / "b");
  write_file(dir / "b" / "keep.txt", "y");

  dirops::Options opt;
  opt.conflict = dirops::ConflictPolicy::Overwrite;
  auto result = dirops::rename_path(dir / "a.txt", dir / "b", opt);
  REQUIRE_FALSE(result.has_value());
  REQUIRE(fs::exists(dir / "b" / "keep.txt"));
  REQUIRE(fs::exists(dir / "a.txt"));

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

TEST_CASE("set_permissions changes mode bits", "[dirops]")
{
  const auto dir = make_temp_dir("dirtoo-test-chmod");
  const auto path = dir / "perm.txt";
  write_file(path, "x");

  auto result = dirops::set_permissions(path, 0640);
  REQUIRE(result.has_value());

  std::error_code ec;
  const auto st = fs::status(path, ec);
  REQUIRE_FALSE(ec);
  const auto perms = st.permissions();
  using fs::perms;
  REQUIRE((perms & perms::owner_read) != perms::none);
  REQUIRE((perms & perms::owner_write) != perms::none);
  REQUIRE((perms & perms::group_read) != perms::none);
  REQUIRE((perms & perms::group_write) == perms::none);
  REQUIRE((perms & perms::others_read) == perms::none);

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

TEST_CASE("move_path onto itself with Overwrite keeps the file", "[dirops][safety]")
{
  const auto dir = make_temp_dir("dirtoo-test-move-self");
  const auto f = dir / "a.txt";
  write_file(f, "precious");

  dirops::Options opts;
  opts.conflict = dirops::ConflictPolicy::Overwrite;
  auto result = dirops::move_path(f, f, opts);
  REQUIRE(result.has_value());
  REQUIRE(fs::exists(f));
  REQUIRE(read_file(f) == "precious");
  fs::remove_all(dir);
}

TEST_CASE("dangling symlinks can be copied and moved", "[dirops][symlink]")
{
  const auto dir = make_temp_dir("dirtoo-test-dangling");
  const auto link = dir / "dangling";
  fs::create_symlink("does-not-exist", link);

  auto copied = dirops::copy_path(link, dir / "copy");
  REQUIRE(copied.has_value());
  REQUIRE(fs::is_symlink(dir / "copy"));
  REQUIRE(fs::read_symlink(dir / "copy") == fs::path("does-not-exist"));

  auto moved = dirops::move_path(link, dir / "moved");
  REQUIRE(moved.has_value());
  REQUIRE(fs::is_symlink(dir / "moved"));
  REQUIRE_FALSE(fs::is_symlink(link));
  fs::remove_all(dir);
}

TEST_CASE("copy_path copies a symlink to a directory as a symlink", "[dirops][symlink]")
{
  const auto dir = make_temp_dir("dirtoo-test-dirlink");
  fs::create_directories(dir / "real");
  write_file(dir / "real" / "f.txt", "x");
  fs::create_directory_symlink("real", dir / "link");

  auto copied = dirops::copy_path(dir / "link", dir / "link2");
  REQUIRE(copied.has_value());
  REQUIRE(fs::is_symlink(dir / "link2"));
  REQUIRE(fs::read_symlink(dir / "link2") == fs::path("real"));
  fs::remove_all(dir);
}

TEST_CASE("copy_path refuses to copy a directory into itself", "[dirops][safety]")
{
  const auto dir = make_temp_dir("dirtoo-test-copy-into-self");
  fs::create_directories(dir / "a" / "b");
  write_file(dir / "a" / "f.txt", "x");

  auto result = dirops::copy_path(dir / "a", dir / "a" / "b");
  REQUIRE_FALSE(result.has_value());
  REQUIRE(fs::exists(dir / "a" / "f.txt"));
  fs::remove_all(dir);
}

TEST_CASE("copy_path reports an unreadable source directory", "[dirops][safety]")
{
  if (::geteuid() == 0) {
    SKIP("permissions are not enforced for root");
  }
  const auto dir = make_temp_dir("dirtoo-test-unreadable");
  fs::create_directories(dir / "locked");
  write_file(dir / "locked" / "f.txt", "x");
  fs::permissions(dir / "locked", fs::perms::none);

  auto result = dirops::copy_path(dir / "locked", dir / "out");
  fs::permissions(dir / "locked", fs::perms::owner_all);
  REQUIRE_FALSE(result.has_value());
  fs::remove_all(dir);
}

TEST_CASE("cancelled cross-device move keeps the source", "[dirops][safety]")
{
  const fs::path shm = "/dev/shm";
  const auto src_dir = make_temp_dir("dirtoo-test-xdev-src");
  if (!fs::is_directory(shm) || dirops::same_filesystem(src_dir, shm)) {
    fs::remove_all(src_dir);
    SKIP("no second filesystem available");
  }
  const auto dst_dir = shm / "dirtoo-test-xdev-dst";
  fs::remove_all(dst_dir);
  fs::create_directories(dst_dir);

  fs::create_directories(src_dir / "tree");
  write_file(src_dir / "tree" / "a.txt", "aaa");
  write_file(src_dir / "tree" / "b.txt", "bbb");

  dirops::Options opts;
  // Cancel only once the move is underway.
  int calls = 0;
  opts.is_cancelled = [&calls] { return ++calls > 3; };

  auto result = dirops::move_path(src_dir / "tree", dst_dir / "tree", opts);
  REQUIRE(result.has_value());
  REQUIRE(result->cancelled);
  REQUIRE(fs::exists(src_dir / "tree" / "a.txt"));
  REQUIRE(fs::exists(src_dir / "tree" / "b.txt"));
  fs::remove_all(src_dir);
  fs::remove_all(dst_dir);
}

TEST_CASE("cross-device move of a directory moves it", "[dirops]")
{
  const fs::path shm = "/dev/shm";
  const auto src_dir = make_temp_dir("dirtoo-test-xdev2-src");
  if (!fs::is_directory(shm) || dirops::same_filesystem(src_dir, shm)) {
    fs::remove_all(src_dir);
    SKIP("no second filesystem available");
  }
  const auto dst_dir = shm / "dirtoo-test-xdev2-dst";
  fs::remove_all(dst_dir);
  fs::create_directories(dst_dir);

  fs::create_directories(src_dir / "tree" / "sub");
  write_file(src_dir / "tree" / "a.txt", "aaa");
  write_file(src_dir / "tree" / "sub" / "b.txt", "bbb");

  auto result = dirops::move_path(src_dir / "tree", dst_dir / "moved");
  REQUIRE(result.has_value());
  REQUIRE_FALSE(fs::exists(src_dir / "tree"));
  REQUIRE(read_file(dst_dir / "moved" / "a.txt") == "aaa");
  REQUIRE(read_file(dst_dir / "moved" / "sub" / "b.txt") == "bbb");
  fs::remove_all(src_dir);
  fs::remove_all(dst_dir);
}

TEST_CASE("copy_path onto itself with Overwrite fails and keeps the file", "[dirops][safety]")
{
  const auto dir = make_temp_dir("dirtoo-test-copy-self");
  const auto f = dir / "a.txt";
  write_file(f, "precious");

  dirops::Options opts;
  opts.conflict = dirops::ConflictPolicy::Overwrite;
  auto result = dirops::copy_path(f, dir, opts);  // dest resolves to dir/a.txt
  REQUIRE_FALSE(result.has_value());
  REQUIRE(read_file(f) == "precious");
  fs::remove_all(dir);
}

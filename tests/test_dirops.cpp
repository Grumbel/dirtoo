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

TEST_CASE("remove_path refuses root, dot and dot-dot", "[dirops][safety]")
{
  const auto dir = make_temp_dir("dirtoo-test-rm-guard");
  fs::create_directories(dir / "sub");
  write_file(dir / "a.txt", "a");
  write_file(dir / "sub" / "b.txt", "b");

  const auto cwd = fs::current_path();
  fs::current_path(dir);
  for (const char* bad : {"", ".", "..", "./", "sub/..", "sub/.", "/", "//", "/."}) {
    INFO("path: '" << bad << "'");
    auto r = dirops::remove_path(bad);
    REQUIRE_FALSE(r.has_value());
  }
  fs::current_path(cwd);

  // Nothing was touched.
  REQUIRE(fs::exists(dir / "a.txt"));
  REQUIRE(fs::exists(dir / "sub" / "b.txt"));

  // Normal removal still works, including a trailing slash.
  REQUIRE(dirops::remove_path(dir / "sub/").has_value());
  REQUIRE_FALSE(fs::exists(dir / "sub"));
  REQUIRE(dirops::remove_path(dir / "a.txt").has_value());
  fs::remove_all(dir);
}

// ---------------------------------------------------------------------------
// freedesktop.org Trash specification
// ---------------------------------------------------------------------------

#include "dirops/trash.hpp"

#include <sys/stat.h>
#include <unistd.h>

namespace {

dirops::TrashOptions trash_opts(const fs::path& base)
{
  dirops::TrashOptions t;
  t.home_trash = base / "Trash";
  t.mount_points = std::vector<fs::path>{};  // no volume trashes unless a test asks
  return t;
}

std::string trashinfo_field(const fs::path& info, const std::string& key)
{
  std::ifstream in(info);
  std::string line;
  while (std::getline(in, line)) {
    if (line.starts_with(key + "=")) {
      return line.substr(key.size() + 1);
    }
  }
  return {};
}

} // namespace

TEST_CASE("trash moves a file into the home trash with a trashinfo", "[dirops][trash]")
{
  const auto dir = make_temp_dir("dirtoo-test-trash-basic");
  const auto t = trash_opts(dir);
  fs::create_directories(dir / "docs");
  const auto file = dir / "docs" / "my report #1.txt";
  write_file(file, "content");

  auto e = dirops::trash_entry(file, t);
  REQUIRE(e.has_value());
  CHECK_FALSE(fs::exists(file));
  CHECK(e->stored_path() == dir / "Trash" / "files" / "my report #1.txt");
  CHECK(read_file(e->stored_path()) == "content");

  // The info file follows the spec: [Trash Info], percent-encoded absolute
  // Path for the home trash, local-time DeletionDate.
  const auto info = e->info_path();
  REQUIRE(fs::exists(info));
  CHECK(read_file(info).starts_with("[Trash Info]\n"));
  CHECK(trashinfo_field(info, "Path") == (dir / "docs").string() + "/my%20report%20%231.txt");
  const auto date = trashinfo_field(info, "DeletionDate");
  REQUIRE(date.size() == 19);
  CHECK(date[4] == '-');
  CHECK(date[10] == 'T');
  CHECK(date[13] == ':');

  // Private directories as required.
  struct stat st {};
  REQUIRE(::stat((dir / "Trash").c_str(), &st) == 0);
  CHECK((st.st_mode & 0777) == 0700);
  fs::remove_all(dir);
}

TEST_CASE("trash picks unique names and never overwrites", "[dirops][trash]")
{
  const auto dir = make_temp_dir("dirtoo-test-trash-names");
  const auto t = trash_opts(dir);
  std::vector<std::string> names;
  for (int i = 0; i < 3; ++i) {
    fs::create_directories(dir / ("d" + std::to_string(i)));
    const auto f = dir / ("d" + std::to_string(i)) / "same.txt";
    write_file(f, "v" + std::to_string(i));
    auto e = dirops::trash_entry(f, t);
    REQUIRE(e.has_value());
    names.push_back(e->name);
    CHECK(read_file(e->stored_path()) == "v" + std::to_string(i));
  }
  CHECK(names[0] == "same.txt");
  CHECK(names[1] != names[0]);
  CHECK(names[2] != names[1]);
  CHECK(names[2] != names[0]);
  fs::remove_all(dir);
}

TEST_CASE("trash handles directories, symlinks and refuses bad targets", "[dirops][trash][safety]")
{
  const auto dir = make_temp_dir("dirtoo-test-trash-kinds");
  const auto t = trash_opts(dir);

  fs::create_directories(dir / "tree" / "sub");
  write_file(dir / "tree" / "sub" / "f", "x");
  auto d = dirops::trash_entry(dir / "tree/", t);  // trailing slash
  REQUIRE(d.has_value());
  CHECK(fs::exists(d->stored_path() / "sub" / "f"));

  // A symlink is trashed as a link; its target is left alone.
  write_file(dir / "target.txt", "t");
  fs::create_symlink(dir / "target.txt", dir / "link");
  auto l = dirops::trash_entry(dir / "link", t);
  REQUIRE(l.has_value());
  CHECK(fs::is_symlink(l->stored_path()));
  CHECK(fs::exists(dir / "target.txt"));

  // Dangling symlinks work too.
  fs::create_symlink("nowhere", dir / "dangling");
  CHECK(dirops::trash_entry(dir / "dangling", t).has_value());

  // Refused: missing path, root, '.' / '..', something already in the trash.
  CHECK_FALSE(dirops::trash_entry(dir / "missing", t).has_value());
  CHECK_FALSE(dirops::trash_entry("/", t).has_value());
  CHECK_FALSE(dirops::trash_entry(dir / "..", t).has_value());
  CHECK_FALSE(dirops::trash_entry(d->stored_path(), t).has_value());
  fs::remove_all(dir);
}

TEST_CASE("trash list, restore, delete and empty", "[dirops][trash]")
{
  const auto dir = make_temp_dir("dirtoo-test-trash-cycle");
  const auto t = trash_opts(dir);
  fs::create_directories(dir / "a" / "b");
  const auto f1 = dir / "a" / "b" / "one.txt";
  const auto f2 = dir / "two.txt";
  write_file(f1, "1");
  write_file(f2, "2");
  REQUIRE(dirops::trash_entry(f1, t).has_value());
  REQUIRE(dirops::trash_entry(f2, t).has_value());

  auto items = dirops::list_trash(t);
  REQUIRE(items.size() == 2);
  auto find = [&](const fs::path& orig) {
    return std::find_if(items.begin(), items.end(),
                        [&](const auto& e) { return e.original_path == orig; });
  };
  REQUIRE(find(f1) != items.end());
  REQUIRE(find(f2) != items.end());
  CHECK(find(f1)->deletion_date.size() == 19);

  // Restore recreates a removed parent directory.
  fs::remove_all(dir / "a");
  auto r = dirops::restore_trash_entry(*find(f1));
  REQUIRE(r.has_value());
  CHECK(read_file(f1) == "1");
  CHECK(dirops::list_trash(t).size() == 1);

  // Restoring onto an existing path fails and keeps the item in the trash.
  write_file(f2, "new two");
  auto clash = dirops::restore_trash_entry(*find(f2));
  REQUIRE_FALSE(clash.has_value());
  CHECK(read_file(f2) == "new two");
  CHECK(dirops::list_trash(t).size() == 1);

  // Permanent delete of one entry, then empty.
  items = dirops::list_trash(t);
  REQUIRE(dirops::delete_trash_entry(items.front()).has_value());
  CHECK(dirops::list_trash(t).empty());

  write_file(dir / "three.txt", "3");
  REQUIRE(dirops::trash_entry(dir / "three.txt", t).has_value());
  // Orphans and the directorysizes cache go as well.
  write_file(dir / "Trash" / "files" / "orphan", "o");
  write_file(dir / "Trash" / "directorysizes", "1 2 x\n");
  REQUIRE(dirops::empty_trash(t).has_value());
  CHECK(dirops::list_trash(t).empty());
  CHECK(fs::is_empty(dir / "Trash" / "files"));
  CHECK(fs::is_empty(dir / "Trash" / "info"));
  CHECK_FALSE(fs::exists(dir / "Trash" / "directorysizes"));
  fs::remove_all(dir);
}

TEST_CASE("trash on another filesystem uses a volume trash directory", "[dirops][trash]")
{
  const fs::path shm = "/dev/shm";
  const auto home = make_temp_dir("dirtoo-test-trash-volume");
  if (!fs::is_directory(shm) || dirops::same_filesystem(home, shm)) {
    fs::remove_all(home);
    SKIP("no second filesystem available");
  }
  const auto vol = shm / ("dirtoo-trash-vol-" + std::to_string(::getpid()));
  fs::remove_all(vol);
  fs::create_directories(vol / "data");
  const auto file = vol / "data" / "x y.txt";
  write_file(file, "vol");

  // Pretend /dev/shm/<vol> is the volume's top directory by listing it as a
  // mount point; the real top directory (/dev/shm) is found by walking up.
  auto t = trash_opts(home);
  t.mount_points = std::vector<fs::path>{shm};
  const uid_t uid = ::geteuid();

  auto e = dirops::trash_entry(file, t);
  REQUIRE(e.has_value());
  CHECK_FALSE(fs::exists(file));
  const auto expected_dir = shm / (".Trash-" + std::to_string(uid));
  CHECK(e->trash_dir == expected_dir);
  CHECK(e->topdir == shm);
  // Volume trashes record the path relative to the top directory.
  const auto rel = trashinfo_field(e->info_path(), "Path");
  CHECK(rel == vol.filename().string() + "/data/x%20y.txt");

  const auto items = dirops::list_trash(t);
  REQUIRE(items.size() == 1);
  CHECK(items[0].original_path == file);
  REQUIRE(dirops::restore_trash_entry(items[0]).has_value());
  CHECK(read_file(file) == "vol");

  fs::remove_all(vol);
  fs::remove_all(expected_dir);
  fs::remove_all(home);
}

TEST_CASE("volume trash: .Trash must be a sticky real directory, else .Trash-$uid",
          "[dirops][trash][safety]")
{
  const fs::path shm = "/dev/shm";
  const auto home = make_temp_dir("dirtoo-test-trash-shared");
  if (!fs::is_directory(shm) || dirops::same_filesystem(home, shm)) {
    fs::remove_all(home);
    SKIP("no second filesystem available");
  }
  const auto top = shm / ("dirtoo-trash-top-" + std::to_string(::getpid()));
  const uid_t uid = ::geteuid();
  auto fresh_top = [&] {
    fs::remove_all(top);
    fs::create_directories(top);
  };
  auto opts = [&] {
    auto t = trash_opts(home);
    t.topdir_override = top;
    t.mount_points = std::vector<fs::path>{top};
    return t;
  };

  // 1. sticky, real directory -> $top/.Trash/$uid
  fresh_top();
  fs::create_directories(top / ".Trash");
  ::chmod((top / ".Trash").c_str(), 01777);
  write_file(top / "a", "a");
  auto e1 = dirops::trash_entry(top / "a", opts());
  REQUIRE(e1.has_value());
  CHECK(e1->trash_dir == top / ".Trash" / std::to_string(uid));
  struct stat st {};
  REQUIRE(::stat(e1->trash_dir.c_str(), &st) == 0);
  CHECK((st.st_mode & 0777) == 0700);
  CHECK(dirops::list_trash(opts()).size() == 1);

  // 2. .Trash without the sticky bit is not trusted -> $top/.Trash-$uid
  fresh_top();
  fs::create_directories(top / ".Trash");
  ::chmod((top / ".Trash").c_str(), 0777);
  write_file(top / "b", "b");
  auto e2 = dirops::trash_entry(top / "b", opts());
  REQUIRE(e2.has_value());
  CHECK(e2->trash_dir == top / (".Trash-" + std::to_string(uid)));

  // 3. .Trash as a symlink is never followed
  fresh_top();
  fs::create_directories(top / "elsewhere");
  ::chmod((top / "elsewhere").c_str(), 01777);
  fs::create_directory_symlink(top / "elsewhere", top / ".Trash");
  write_file(top / "c", "c");
  auto e3 = dirops::trash_entry(top / "c", opts());
  REQUIRE(e3.has_value());
  CHECK(e3->trash_dir == top / (".Trash-" + std::to_string(uid)));
  CHECK(fs::is_empty(top / "elsewhere"));

  // 4. an existing .Trash-$uid owned by someone else is not used
  //    (cannot chown in a test; check that a *symlinked* one is refused)
  fresh_top();
  fs::create_directories(top / "decoy");
  fs::create_directory_symlink(top / "decoy", top / (".Trash-" + std::to_string(uid)));
  write_file(top / "d", "d");
  CHECK_FALSE(dirops::trash_entry(top / "d", opts()).has_value());
  CHECK(fs::exists(top / "d"));
  CHECK(fs::is_empty(top / "decoy"));

  fs::remove_all(top);
  fs::remove_all(home);
}

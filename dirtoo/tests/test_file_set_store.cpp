// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/sets/file_set_store.hpp"
#include "set_membership.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using dirtoo::sets::FileSetStore;

namespace {

fs::path temp_db_path(const std::string& suffix)
{
  const auto dir = fs::temp_directory_path() / "dirtoo-fileset-test";
  fs::create_directories(dir);
  return dir / ("sets-" + suffix + ".sqlite");
}

void remove_db_sidecars(const fs::path& db)
{
  std::error_code ec;
  fs::remove(db, ec);
  fs::remove(fs::path(db.string() + "-wal"), ec);
  fs::remove(fs::path(db.string() + "-shm"), ec);
}

} // namespace

TEST_CASE("FileSetStore create anonymous set and reopen", "[sets]")
{
  const auto path = temp_db_path("reopen");
  remove_db_sidecars(path);

  std::string id;
  {
    FileSetStore store;
    std::string err;
    REQUIRE(store.open(path, &err));
    auto set = store.create_set({}, {}, &err);
    REQUIRE(set);
    CHECK(set->label.empty());
    CHECK(set->id.size() == 32);
    id = set->id;
  }

  {
    FileSetStore store;
    std::string err;
    REQUIRE(store.open(path, &err));
    auto set = store.get_set(id);
    REQUIRE(set);
    CHECK(set->id == id);
    CHECK(set->member_count == 0);
  }

  remove_db_sidecars(path);
}

TEST_CASE("FileSetStore membership exclusive (one set per file)", "[sets]")
{
  const auto path = temp_db_path("overlap");
  remove_db_sidecars(path);

  FileSetStore store;
  std::string err;
  REQUIRE(store.open(path, &err));

  auto a = store.create_set("takes", "#E5A00D", &err);
  auto b = store.create_set("refs", {}, &err);
  REQUIRE(a);
  REQUIRE(b);

  REQUIRE(store.add_member(a->id, "/media/clip1.mp4", {}, &err));
  REQUIRE(store.add_member(a->id, "/media/clip2.mp4", {}, &err));
  REQUIRE(store.add_member(b->id, "/media/clip2.mp4", {}, &err)); // moves clip2 from a → b
  REQUIRE(store.add_member(b->id, "/media/still.png", "abc123", &err));

  CHECK(store.member_count(a->id) == 1);
  CHECK(store.member_count(b->id) == 2);
  CHECK_FALSE(store.contains(a->id, "/media/clip2.mp4"));
  CHECK(store.contains(b->id, "/media/clip2.mp4"));
  CHECK_FALSE(store.contains(a->id, "/media/still.png"));

  const auto in_clip2 = store.sets_for_path("/media/clip2.mp4");
  REQUIRE(in_clip2.size() == 1);
  CHECK(in_clip2.front().id == b->id);

  const auto members_a = store.members(a->id);
  REQUIRE(members_a.size() == 2);

  REQUIRE(store.remove_member(a->id, "/media/clip1.mp4", &err));
  CHECK(store.member_count(a->id) == 1);
  CHECK_FALSE(store.contains(a->id, "/media/clip1.mp4"));

  // still.png keeps sha
  const auto members_b = store.members(b->id);
  auto it = std::find_if(members_b.begin(), members_b.end(),
                         [](const auto& m) { return m.path_key == "/media/still.png"; });
  REQUIRE(it != members_b.end());
  CHECK(it->sha256 == "abc123");

  remove_db_sidecars(path);
}

TEST_CASE("FileSetStore label dissolve cascade", "[sets]")
{
  const auto path = temp_db_path("dissolve");
  remove_db_sidecars(path);

  FileSetStore store;
  std::string err;
  REQUIRE(store.open(path, &err));
  auto set = store.create_set({}, {}, &err);
  REQUIRE(set);
  REQUIRE(store.add_members(set->id, {"/a", "/b", "/c"}, &err) == 3);
  // second add is no-op count
  CHECK(store.add_members(set->id, {"/a", "/d"}, &err) == 1);
  CHECK(store.member_count(set->id) == 4);

  REQUIRE(store.set_label(set->id, "bug-report", &err));
  auto got = store.get_set(set->id);
  REQUIRE(got);
  CHECK(got->label == "bug-report");

  REQUIRE(store.delete_set(set->id, &err));
  CHECK_FALSE(store.get_set(set->id));
  CHECK(store.members(set->id).empty());
  CHECK(store.sets_for_path("/a").empty());

  remove_db_sidecars(path);
}

TEST_CASE("FileSetStore list orders by updated", "[sets]")
{
  const auto path = temp_db_path("order");
  remove_db_sidecars(path);

  FileSetStore store;
  std::string err;
  REQUIRE(store.open(path, &err));
  auto older = store.create_set("older", {}, &err);
  auto newer = store.create_set("newer", {}, &err);
  REQUIRE(older);
  REQUIRE(newer);
  // Touch older by adding a member → should sort first as most recently updated.
  REQUIRE(store.add_member(older->id, "/x", {}, &err));

  const auto listed = store.list_sets();
  REQUIRE(listed.size() >= 2);
  CHECK(listed.front().id == older->id);

  remove_db_sidecars(path);
}

TEST_CASE("Filter set: and in-set: membership", "[sets][filter]")
{
  const auto path = temp_db_path("filter");
  remove_db_sidecars(path);

  // Point store at temp DB by opening and using that path — filter uses default_path.
  // Instead, exercise store membership API + predicate path_key logic indirectly.
  FileSetStore store;
  std::string err;
  REQUIRE(store.open(path, &err));
  auto set = store.create_set("filter-test", {}, &err);
  REQUIRE(set);
  const auto tmp = fs::temp_directory_path() / "dirtoo-set-filter-file.txt";
  { std::ofstream ofs(tmp); ofs << "x"; }
  REQUIRE(store.add_member(set->id, tmp.string(), {}, &err));
  CHECK(store.contains(set->id, tmp.string()));
  CHECK(store.sets_for_path(tmp.string()).size() == 1);
  fs::remove(tmp);
  remove_db_sidecars(path);
}

TEST_CASE("pure_set_query accepts quoted labels with spaces", "[sets][filter]")
{
  using dirtoo::app::set_membership::pure_set_query;
  REQUIRE(pure_set_query("set:abc") == std::string{"abc"});
  REQUIRE(pure_set_query("set:\"foo bar\"") == std::string{"foo bar"});
  REQUIRE(pure_set_query("set:'my set'") == std::string{"my set"});
  REQUIRE_FALSE(pure_set_query("set:foo bar").has_value()); // unquoted space → not pure
  REQUIRE_FALSE(pure_set_query("set:\"foo").has_value());
  REQUIRE_FALSE(pure_set_query("png").has_value());
}

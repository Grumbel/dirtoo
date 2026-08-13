// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/tags/tag_store.hpp"
#include "dirtoo/tags/tag_def.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using dirtoo::tags::TagStore;
using dirtoo::tags::normalize_tag_name;

namespace {

fs::path temp_db_path(const std::string& suffix)
{
  const auto dir = fs::temp_directory_path() / "dirtoo-tagstore-test";
  fs::create_directories(dir);
  return dir / ("tags-" + suffix + ".sqlite");
}

void remove_db_sidecars(const fs::path& db)
{
  std::error_code ec;
  fs::remove(db, ec);
  fs::remove(fs::path(db.string() + "-wal"), ec);
  fs::remove(fs::path(db.string() + "-shm"), ec);
}

} // namespace

TEST_CASE("normalize_tag_name basics", "[tags]")
{
  CHECK(normalize_tag_name("Work") == "work");
  CHECK(normalize_tag_name("  Foo/Bar  ") == "foo/bar");
  CHECK(normalize_tag_name("ns:Local") == "ns:local");
  CHECK(normalize_tag_name("").empty());
  CHECK(normalize_tag_name("!!!").empty());
}

TEST_CASE("TagStore open create schema and reopen", "[tags]")
{
  const auto path = temp_db_path("reopen");
  remove_db_sidecars(path);

  {
    TagStore store;
    std::string err;
    REQUIRE(store.open(path, &err));
    CHECK(store.is_open());
    CHECK(store.path() == path);
    auto def = store.ensure_tag("alpha", &err);
    REQUIRE(def);
    CHECK(def->name == "alpha");
  }

  {
    TagStore store;
    std::string err;
    REQUIRE(store.open(path, &err));
    auto def = store.get_tag("alpha");
    REQUIRE(def);
    CHECK(def->name == "alpha");
    auto listed = store.list_tags();
    REQUIRE(listed.size() >= 1);
  }

  remove_db_sidecars(path);
}

TEST_CASE("TagStore tag file and cascade delete", "[tags]")
{
  const auto path = temp_db_path("cascade");
  remove_db_sidecars(path);

  TagStore store;
  std::string err;
  REQUIRE(store.open(path, &err));

  REQUIRE(store.ensure_tag("keep", &err));
  REQUIRE(store.ensure_tag("drop", &err));

  const std::string sha =
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
  auto file_id = store.ensure_file_sha256(sha, "/tmp/empty.bin", &err);
  REQUIRE(file_id);
  REQUIRE(store.add_tag_to_file(*file_id, "keep", &err));
  REQUIRE(store.add_tag_to_file(*file_id, "drop", &err));

  auto tags = store.tags_for_file(*file_id);
  REQUIRE(tags.size() == 2);

  REQUIRE(store.delete_tag("drop", &err));
  tags = store.tags_for_file(*file_id);
  REQUIRE(tags.size() == 1);
  CHECK(tags[0] == "keep");
  CHECK(store.count_files_for_tag("drop") == 0);
  CHECK(store.count_files_for_tag("keep") == 1);

  remove_db_sidecars(path);
}

TEST_CASE("TagStore rename keeps file associations", "[tags]")
{
  const auto path = temp_db_path("rename");
  remove_db_sidecars(path);

  TagStore store;
  std::string err;
  REQUIRE(store.open(path, &err));
  REQUIRE(store.ensure_tag("oldname", &err));

  const std::string sha =
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
  auto file_id = store.ensure_file_sha256(sha, {}, &err);
  REQUIRE(file_id);
  REQUIRE(store.add_tag_to_file(*file_id, "oldname", &err));
  REQUIRE(store.rename_tag("oldname", "newname", &err));

  CHECK_FALSE(store.get_tag("oldname"));
  REQUIRE(store.get_tag("newname"));
  auto tags = store.tags_for_file(*file_id);
  REQUIRE(tags.size() == 1);
  CHECK(tags[0] == "newname");

  remove_db_sidecars(path);
}

TEST_CASE("TagStore concurrent open smoke (WAL)", "[tags][concurrent]")
{
  const auto path = temp_db_path("concurrent");
  remove_db_sidecars(path);

  // Seed schema + a few rows on the primary connection.
  {
    TagStore seed;
    std::string err;
    REQUIRE(seed.open(path, &err));
    REQUIRE(seed.ensure_tag("shared", &err));
    for (int i = 0; i < 8; ++i) {
      // Distinct fake digests (64 hex chars).
      std::string sha(64, '0');
      sha[63] = static_cast<char>('a' + (i % 6));
      sha[62] = static_cast<char>('0' + (i % 10));
      auto id = seed.ensure_file_sha256(sha, {}, &err);
      REQUIRE(id);
      REQUIRE(seed.add_tag_to_file(*id, "shared", &err));
    }
  }

  constexpr int kReaders = 4;
  constexpr int kWriters = 2;
  std::atomic<int> read_ok{0};
  std::atomic<int> write_ok{0};
  std::atomic<int> failures{0};
  std::vector<std::thread> threads;

  for (int r = 0; r < kReaders; ++r) {
    threads.emplace_back([&, r] {
      TagStore store;
      std::string err;
      if (!store.open(path, &err)) {
        ++failures;
        return;
      }
      for (int i = 0; i < 40; ++i) {
        const auto tags = store.list_tags();
        if (tags.empty()) {
          ++failures;
          return;
        }
        (void)store.count_files_for_tag("shared");
        (void)store.files_for_tag("shared");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      ++read_ok;
      (void)r;
    });
  }

  for (int w = 0; w < kWriters; ++w) {
    threads.emplace_back([&, w] {
      TagStore store;
      std::string err;
      if (!store.open(path, &err)) {
        ++failures;
        return;
      }
      for (int i = 0; i < 20; ++i) {
        const std::string name = "w" + std::to_string(w) + "_" + std::to_string(i);
        auto def = store.ensure_tag(name, &err);
        if (!def) {
          ++failures;
          return;
        }
        std::string sha(64, '1');
        sha[0] = static_cast<char>('a' + (w % 6));
        sha[1] = static_cast<char>('0' + (i % 10));
        sha[2] = static_cast<char>('0' + (w % 10));
        auto id = store.ensure_file_sha256(sha, {}, &err);
        if (!id || !store.add_tag_to_file(*id, name, &err)) {
          ++failures;
          return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      ++write_ok;
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  CHECK(failures.load() == 0);
  CHECK(read_ok.load() == kReaders);
  CHECK(write_ok.load() == kWriters);

  // Final consistency: shared tag still present.
  TagStore check;
  std::string err;
  REQUIRE(check.open(path, &err));
  REQUIRE(check.get_tag("shared"));
  CHECK(check.count_files_for_tag("shared") >= 8);

  remove_db_sidecars(path);
}

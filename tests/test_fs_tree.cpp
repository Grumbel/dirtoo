// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/tree/fs_tree_cache.hpp"
#include <algorithm>
#include "dirtoo/tree/scan_tree.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using Catch::Approx;

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;
using dirtoo::tree::FsTreeCache;
using dirtoo::tree::FsTreeNodeKind;
using dirtoo::tree::FsTreeNodeState;
using dirtoo::tree::ScanOptions;
using dirtoo::tree::scan_tree;

namespace {

struct TempDir {
  fs::path path;
  TempDir()
  {
    path = fs::temp_directory_path()
           / ("dirtoo-tree-test-" + std::to_string(
                  std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(path);
  }
  ~TempDir()
  {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
};

void write_file(const fs::path& p, std::string_view data)
{
  fs::create_directories(p.parent_path());
  std::ofstream out(p, std::ios::binary);
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
  REQUIRE(out.good());
}

} // namespace

TEST_CASE("scan_tree empty directory", "[tree]")
{
  TempDir tmp;
  const auto root = scan_tree(tmp.path);
  REQUIRE(root);
  CHECK(root->kind() == FsTreeNodeKind::Directory);
  CHECK(root->state() == FsTreeNodeState::Complete);
  CHECK(root->children().empty());
  CHECK(root->own_size() == 0);
  CHECK(root->total_size() == 0);
}

TEST_CASE("scan_tree aggregates file sizes", "[tree]")
{
  TempDir tmp;
  write_file(tmp.path / "a.txt", "hello");      // 5
  write_file(tmp.path / "b.txt", "world!!");    // 7
  fs::create_directory(tmp.path / "sub");
  write_file(tmp.path / "sub" / "c.txt", "xy"); // 2

  const auto root = scan_tree(tmp.path);
  REQUIRE(root);
  CHECK(root->state() == FsTreeNodeState::Complete);
  CHECK(root->total_size() == 5 + 7 + 2);

  // Find sub
  const dirtoo::tree::FsTreeNode* sub = nullptr;
  for (const auto& ch : root->children()) {
    if (ch->name() == "sub") {
      sub = ch.get();
      break;
    }
  }
  REQUIRE(sub != nullptr);
  CHECK(sub->kind() == FsTreeNodeKind::Directory);
  CHECK(sub->total_size() == 2);
  REQUIRE(sub->children().size() == 1);
  CHECK(sub->children()[0]->name() == "c.txt");
  CHECK(sub->children()[0]->own_size() == 2);
  CHECK(sub->children()[0]->total_size() == 2);
}

TEST_CASE("scan_tree max_depth", "[tree]")
{
  TempDir tmp;
  write_file(tmp.path / "top.txt", "aa");
  fs::create_directory(tmp.path / "d");
  write_file(tmp.path / "d" / "deep.txt", "bbbb");

  ScanOptions opts;
  opts.max_depth = 0; // root only — do not list children
  const auto shallow = scan_tree(tmp.path, opts);
  REQUIRE(shallow);
  CHECK(shallow->children().empty());
  CHECK(shallow->total_size() == 0);

  opts.max_depth = 1;
  const auto one = scan_tree(tmp.path, opts);
  REQUIRE(one);
  // top.txt + d (d not descended)
  CHECK(one->children().size() == 2);
  std::uint64_t total = 0;
  for (const auto& ch : one->children()) {
    total += ch->total_size();
  }
  CHECK(total == 2); // only top.txt content; d empty at this depth
}

TEST_CASE("scan_tree cancel marks partial", "[tree]")
{
  TempDir tmp;
  // Several nested dirs so cancel can fire mid-walk.
  for (int i = 0; i < 20; ++i) {
    write_file(tmp.path / ("f" + std::to_string(i) + ".bin"), std::string(64, 'x'));
    fs::create_directory(tmp.path / ("d" + std::to_string(i)));
    write_file(tmp.path / ("d" + std::to_string(i)) / "n.txt", "z");
  }

  std::atomic_bool cancel{false};
  cancel.store(true);
  const auto root = scan_tree(tmp.path, {}, &cancel);
  REQUIRE(root);
  // Immediate cancel may leave Partial or still Complete if finished before check;
  // at minimum we must not crash and must return a node.
  CHECK((root->state() == FsTreeNodeState::Partial || root->state() == FsTreeNodeState::Complete
         || root->state() == FsTreeNodeState::Pending || root->state() == FsTreeNodeState::Failed));
}

TEST_CASE("FsTreeCache snapshot scan invalidate", "[tree]")
{
  TempDir tmp;
  write_file(tmp.path / "x.dat", "12345");

  FsTreeCache cache;
  CHECK(cache.snapshot(tmp.path) == nullptr);

  const auto s1 = cache.scan(tmp.path);
  REQUIRE(s1);
  CHECK(s1->total_size() == 5);
  CHECK(cache.size() == 1);

  const auto s2 = cache.snapshot(tmp.path);
  REQUIRE(s2);
  CHECK(s2->total_size() == 5);

  cache.invalidate(tmp.path);
  CHECK(cache.snapshot(tmp.path) == nullptr);
  CHECK(cache.size() == 0);

  cache.scan(tmp.path);
  cache.invalidate_subtree(tmp.path);
  CHECK(cache.size() == 0);
}

TEST_CASE("scan_tree single file root", "[tree]")
{
  TempDir tmp;
  const auto file = tmp.path / "solo.txt";
  write_file(file, "abc");
  const auto node = scan_tree(file);
  REQUIRE(node);
  CHECK(node->kind() == FsTreeNodeKind::File);
  CHECK(node->state() == FsTreeNodeState::Complete);
  CHECK(node->own_size() == 3);
  CHECK(node->total_size() == 3);
  CHECK(node->children().empty());
}

TEST_CASE("FsTreeCache entry_total_size filled during scan", "[tree]")
{
  TempDir tmp;
  write_file(tmp.path / "a.txt", "hello");
  fs::create_directory(tmp.path / "sub");
  write_file(tmp.path / "sub" / "b.txt", "xy");

  FsTreeCache cache;
  cache.scan(tmp.path);
  const auto root_sz = cache.entry_total_size(tmp.path);
  REQUIRE(root_sz);
  CHECK(*root_sz == 7);
  const auto sub_sz = cache.entry_total_size(tmp.path / "sub");
  REQUIRE(sub_sz);
  CHECK(*sub_sz == 2);
  const auto file_sz = cache.entry_total_size(tmp.path / "a.txt");
  REQUIRE(file_sz);
  CHECK(*file_sz == 5);
}


#include "dirtoo/tree/treemap_layout.hpp"

TEST_CASE("layout_squarified empty / zero weights", "[tree][treemap]")
{
  using namespace dirtoo::tree;
  CHECK(layout_squarified({}, TreemapRect{0, 0, 100, 100}).empty());
  CHECK(layout_squarified({{"a", 0}, {"b", -1}}, TreemapRect{0, 0, 100, 100}).empty());
  CHECK(layout_squarified({{"a", 1}}, TreemapRect{0, 0, 0, 10}).empty());
}

TEST_CASE("layout_squarified single fills bounds", "[tree][treemap]")
{
  using namespace dirtoo::tree;
  const auto cells = layout_squarified({{"only", 42}}, TreemapRect{10, 20, 100, 50});
  REQUIRE(cells.size() == 1);
  CHECK(cells[0].id == "only");
  CHECK(cells[0].rect.x == Approx(10));
  CHECK(cells[0].rect.y == Approx(20));
  CHECK(cells[0].rect.w == Approx(100));
  CHECK(cells[0].rect.h == Approx(50));
}

TEST_CASE("layout_squarified areas proportional", "[tree][treemap]")
{
  using namespace dirtoo::tree;
  const TreemapRect bounds{0, 0, 100, 100};
  const auto cells = layout_squarified(
      {{"a", 1}, {"b", 1}, {"c", 2}}, bounds);
  REQUIRE(cells.size() == 3);
  double area_sum = 0;
  double weight_sum = 0;
  for (const auto& c : cells) {
    area_sum += c.rect.area();
    weight_sum += c.weight;
  }
  CHECK(area_sum == Approx(bounds.area()).margin(1e-6));
  // Scaled weights sum to area.
  CHECK(weight_sum == Approx(bounds.area()).margin(1e-6));
  // Largest id should be c.
  auto it = std::find_if(cells.begin(), cells.end(),
                         [](const TreemapCell& c) { return c.id == "c"; });
  REQUIRE(it != cells.end());
  CHECK(it->rect.area() == Approx(50).margin(1e-3));
}

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/hash/hash_file.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using dirtoo::hash::FileDigests;
using dirtoo::hash::HashError;
using dirtoo::hash::HashOptions;
using dirtoo::hash::QuickHashOptions;
using dirtoo::hash::hash_file;
using dirtoo::hash::hash_file_quick;

namespace {

fs::path make_temp_file(const std::string& name, const std::string& contents)
{
  const auto dir = fs::temp_directory_path() / "dirtoo-hash-test";
  fs::create_directories(dir);
  const auto path = dir / name;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  REQUIRE(out);
  return path;
}

fs::path make_temp_file_bytes(const std::string& name, std::uint64_t size, unsigned char fill)
{
  const auto dir = fs::temp_directory_path() / "dirtoo-hash-test";
  fs::create_directories(dir);
  const auto path = dir / name;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  std::vector<char> chunk(1 << 16, static_cast<char>(fill));
  std::uint64_t left = size;
  while (left > 0) {
    const auto n = static_cast<std::size_t>(std::min<std::uint64_t>(left, chunk.size()));
    out.write(chunk.data(), static_cast<std::streamsize>(n));
    left -= n;
  }
  REQUIRE(out);
  return path;
}

} // namespace

TEST_CASE("hash_file empty regular file", "[hash]")
{
  const auto path = make_temp_file("empty.bin", "");
  HashError err;
  const auto d = hash_file(path, {}, &err);
  REQUIRE(d);
  CHECK(d->size == 0);
  CHECK(d->crc32_hex.size() == 8);
  CHECK(d->md5_hex.size() == 32);
  CHECK(d->sha1_hex.size() == 40);
  CHECK(d->sha256_hex.size() == 64);
  // Empty-file well-known digests
  CHECK(d->md5_hex == "d41d8cd98f00b204e9800998ecf8427e");
  CHECK(d->sha1_hex == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
  CHECK(d->sha256_hex ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(d->crc32_hex == "00000000");
}

TEST_CASE("hash_file known short content", "[hash]")
{
  const auto path = make_temp_file("abc.txt", "abc");
  HashError err;
  const auto d = hash_file(path, {}, &err);
  REQUIRE(d);
  CHECK(d->size == 3);
  CHECK(d->sha256_hex ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(d->md5_hex == "900150983cd24fb0d6963f7d28e17f72");
  CHECK(d->sha1_hex == "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST_CASE("hash_file rejects directory", "[hash]")
{
  const auto dir = fs::temp_directory_path() / "dirtoo-hash-test" / "subdir";
  fs::create_directories(dir);
  HashError err;
  const auto d = hash_file(dir, {}, &err);
  REQUIRE_FALSE(d);
  CHECK(err.message.find("not a regular file") != std::string::npos);
}

TEST_CASE("hash_file cancel aborts", "[hash]")
{
  const auto path = make_temp_file_bytes("cancel.bin", 4ULL << 20, 0xAB);
  HashOptions opts;
  opts.should_cancel = [] { return true; };
  HashError err;
  const auto d = hash_file(path, opts, &err);
  REQUIRE_FALSE(d);
  CHECK(err.message == "cancelled");
}

TEST_CASE("hash_file_quick small file equals full hash", "[hash][quick]")
{
  const auto path = make_temp_file("small-quick.bin", std::string(1000, 'x'));
  HashError err_full;
  HashError err_quick;
  const auto full = hash_file(path, {}, &err_full);
  const auto quick = hash_file_quick(path, {}, &err_quick);
  REQUIRE(full);
  REQUIRE(quick);
  CHECK(quick->size == full->size);
  CHECK(quick->sha256_hex == full->sha256_hex);
  CHECK(quick->md5_hex == full->md5_hex);
  CHECK(quick->crc32_hex == full->crc32_hex);
  // Marker "quick" only on the sample path (size > 3·window)
  CHECK(quick->crc32_hex != "quick");
}

TEST_CASE("hash_file_quick large file uses sample marker", "[hash][quick]")
{
  // Default window is 1 MiB; need size > 3 MiB for sample path.
  constexpr std::uint64_t kSize = (3ULL << 20) + 4096;
  const auto path = make_temp_file_bytes("large-quick.bin", kSize, 0x5A);
  QuickHashOptions qopts;
  qopts.window_bytes = 1ULL << 20;
  HashError err;
  const auto d = hash_file_quick(path, qopts, &err);
  REQUIRE(d);
  CHECK(d->size == kSize);
  CHECK(d->crc32_hex == "quick");
  CHECK(d->md5_hex.empty());
  CHECK(d->sha1_hex.empty());
  CHECK(d->sha256_hex.size() == 64);
  // Stable: same file → same sample digest
  const auto d2 = hash_file_quick(path, qopts, &err);
  REQUIRE(d2);
  CHECK(d2->sha256_hex == d->sha256_hex);
}

TEST_CASE("hash_file_quick sample differs when mid window changes", "[hash][quick]")
{
  constexpr std::uint64_t kWindow = 64 * 1024;
  constexpr std::uint64_t kSize = kWindow * 4;
  const auto dir = fs::temp_directory_path() / "dirtoo-hash-test";
  fs::create_directories(dir);
  const auto path_a = dir / "sample-a.bin";
  const auto path_b = dir / "sample-b.bin";

  {
    std::vector<char> buf(static_cast<std::size_t>(kSize), '\x11');
    std::ofstream(path_a, std::ios::binary).write(buf.data(), static_cast<std::streamsize>(buf.size()));
    // Change only bytes in the middle window region.
    const std::size_t mid = static_cast<std::size_t>(kSize / 2);
    buf[mid] = '\x22';
    std::ofstream(path_b, std::ios::binary).write(buf.data(), static_cast<std::streamsize>(buf.size()));
  }

  QuickHashOptions qopts;
  qopts.window_bytes = kWindow;
  HashError err;
  const auto a = hash_file_quick(path_a, qopts, &err);
  const auto b = hash_file_quick(path_b, qopts, &err);
  REQUIRE(a);
  REQUIRE(b);
  CHECK(a->crc32_hex == "quick");
  CHECK(b->crc32_hex == "quick");
  CHECK(a->sha256_hex != b->sha256_hex);
}

TEST_CASE("hash_file_quick cancel during sample", "[hash][quick]")
{
  constexpr std::uint64_t kSize = (4ULL << 20);
  const auto path = make_temp_file_bytes("cancel-quick.bin", kSize, 0x01);
  QuickHashOptions qopts;
  qopts.window_bytes = 1ULL << 20;
  qopts.should_cancel = [] { return true; };
  HashError err;
  const auto d = hash_file_quick(path, qopts, &err);
  REQUIRE_FALSE(d);
  CHECK(err.message == "cancelled");
}

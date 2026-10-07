// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "atomic_write.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using dirtoo::app::write_file_atomic;

namespace {

fs::path fresh_dir(const char* tag)
{
  const auto dir = fs::temp_directory_path()
                   / (std::string("dirtoo-atomic-") + tag + "-" + std::to_string(::getpid()));
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir);
  return dir;
}

std::string slurp(const fs::path& p)
{
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), {});
}

} // namespace

TEST_CASE("write_file_atomic creates parents and replaces content", "[atomic]")
{
  const auto dir = fresh_dir("basic");
  const auto file = dir / "nested" / "deeper" / "data.txt";
  REQUIRE(write_file_atomic(file, "first\n"));
  CHECK(slurp(file) == "first\n");
  REQUIRE(write_file_atomic(file, "second\n"));
  CHECK(slurp(file) == "second\n");

  // No temporary files left behind.
  int entries = 0;
  for (const auto& e : fs::directory_iterator(file.parent_path())) {
    (void)e;
    ++entries;
  }
  CHECK(entries == 1);
  fs::remove_all(dir);
}

TEST_CASE("write_file_atomic keeps the old file when it cannot write", "[atomic][safety]")
{
  if (::geteuid() == 0) {
    SKIP("permissions are not enforced for root");
  }
  const auto dir = fresh_dir("keep");
  const auto file = dir / "bookmarks";
  REQUIRE(write_file_atomic(file, "precious\n"));

  // Directory not writable: the temporary file cannot be created.
  fs::permissions(dir, fs::perms::owner_read | fs::perms::owner_exec);
  std::string err;
  const bool ok = write_file_atomic(file, "replacement\n", &err);
  fs::permissions(dir, fs::perms::owner_all);

  CHECK_FALSE(ok);
  CHECK_FALSE(err.empty());
  CHECK(slurp(file) == "precious\n");  // `ofstream(path, trunc)` would have emptied it
  fs::remove_all(dir);
}

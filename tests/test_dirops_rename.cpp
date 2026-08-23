// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/ops.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("rename_path moves a file", "[dirops]")
{
  const auto dir = fs::temp_directory_path() / "dirtoo-test-rename";
  fs::remove_all(dir);
  fs::create_directories(dir);

  const auto from = dir / "a.txt";
  const auto to = dir / "b.txt";
  {
    std::ofstream out(from);
    out << "hello";
  }

  auto result = dirops::rename_path(from, to);
  REQUIRE(result.has_value());
  REQUIRE(fs::exists(to));
  REQUIRE_FALSE(fs::exists(from));

  fs::remove_all(dir);
}

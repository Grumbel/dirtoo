// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/fs/location.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Location from_path and parent", "[location]")
{
  const auto loc = dirtoo::fs::Location::from_path("/tmp");
  REQUIRE(loc.basename() == "tmp");
  REQUIRE(loc.as_url().starts_with("file://"));

  const auto parent = loc.parent();
  REQUIRE(parent.as_path() == "/");
}

TEST_CASE("Location join", "[location]")
{
  const auto loc = dirtoo::fs::Location::from_path("/tmp");
  const auto child = loc.join("foo");
  REQUIRE(child.as_path() == "/tmp/foo");
}

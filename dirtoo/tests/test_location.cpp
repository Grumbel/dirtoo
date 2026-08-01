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

TEST_CASE("archive location url roundtrip", "[location]")
{
  const auto loc = dirtoo::fs::Location::from_archive("/tmp/demo.zip", "docs/readme.txt");
  REQUIRE(loc.is_archive());
  REQUIRE(loc.as_path().filename() == "demo.zip");
  REQUIRE(loc.entry_path() == "docs/readme.txt");

  const auto again = dirtoo::fs::Location::from_url(loc.as_url());
  REQUIRE(again.is_archive());
  REQUIRE(again.as_path() == loc.as_path());
  REQUIRE(again.entry_path() == loc.entry_path());
}

TEST_CASE("looks_like_archive", "[location]")
{
  REQUIRE(dirtoo::fs::looks_like_archive("a.zip"));
  REQUIRE(dirtoo::fs::looks_like_archive("b.tar.gz"));
  REQUIRE_FALSE(dirtoo::fs::looks_like_archive("c.txt"));
}

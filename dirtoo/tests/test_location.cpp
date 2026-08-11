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

  // Preferred Python-style encoding.
  const std::string url = loc.as_url();
  REQUIRE(url.find("file://") == 0);
  REQUIRE(url.find("//archive:") != std::string::npos);
  REQUIRE(url.find("docs/readme.txt") != std::string::npos);

  const auto again = dirtoo::fs::Location::from_url(url);
  REQUIRE(again.is_archive());
  REQUIRE(again.as_path() == loc.as_path());
  REQUIRE(again.entry_path() == loc.entry_path());

  // Legacy JAR-style still accepted.
  const auto legacy = dirtoo::fs::Location::from_url("archive:///tmp/demo.zip!/docs/readme.txt");
  REQUIRE(legacy.is_archive());
  REQUIRE(legacy.entry_path() == "docs/readme.txt");

  // Archive root without entry.
  const auto root = dirtoo::fs::Location::from_archive("/tmp/demo.zip", "");
  REQUIRE(root.as_url().ends_with("//archive"));
  const auto root2 = dirtoo::fs::Location::from_url(root.as_url());
  REQUIRE(root2.is_archive());
  REQUIRE(root2.entry_path().empty());
}

TEST_CASE("looks_like_archive", "[location]")
{
  REQUIRE(dirtoo::fs::looks_like_archive("a.zip"));
  REQUIRE(dirtoo::fs::looks_like_archive("b.tar.gz"));
  REQUIRE_FALSE(dirtoo::fs::looks_like_archive("c.txt"));
}

TEST_CASE("archive parent leaves archive at root", "[location]")
{
  const auto root = dirtoo::fs::Location::from_archive("/tmp/demo.zip", "");
  const auto parent = root.parent();
  REQUIRE(parent.is_file());
  REQUIRE(parent.as_path().filename() != "demo.zip");
  REQUIRE_FALSE(parent.is_archive());

  const auto nested = dirtoo::fs::Location::from_archive("/tmp/demo.zip", "a/b");
  const auto up = nested.parent();
  REQUIRE(up.is_archive());
  REQUIRE(up.entry_path() == "a");
}

TEST_CASE("Location percent-encoding roundtrip", "[location]")
{
  // Space
  {
    const auto loc = dirtoo::fs::Location::from_path_unchecked("/tmp/my file.txt");
    const std::string url = loc.as_url();
    REQUIRE(url.find("%20") != std::string::npos);
    REQUIRE(url.find(' ') == std::string::npos);
    const auto again = dirtoo::fs::Location::from_url(url);
    REQUIRE(again.as_path() == loc.as_path());
  }
  // Hash and question mark (would break naive URL parsing / cache keys)
  {
    const auto loc = dirtoo::fs::Location::from_path_unchecked("/tmp/a#b?c.txt");
    const std::string url = loc.as_url();
    REQUIRE(url.find("%23") != std::string::npos);
    REQUIRE(url.find("%3F") != std::string::npos);
    const auto again = dirtoo::fs::Location::from_url(url);
    REQUIRE(again.as_path() == loc.as_path());
  }
  // Archive entry with space
  {
    const auto loc = dirtoo::fs::Location::from_archive("/tmp/demo.zip", "docs/my notes.txt");
    const std::string url = loc.as_url();
    REQUIRE(url.find("%20") != std::string::npos);
    const auto again = dirtoo::fs::Location::from_url(url);
    REQUIRE(again.is_archive());
    REQUIRE(again.entry_path() == loc.entry_path());
  }
}

TEST_CASE("from_human preserves archive payload", "[location]")
{
  // Bare path form users type / see in some UI paths.
  const auto a = dirtoo::fs::Location::from_human("/tmp/demo.zip//archive");
  REQUIRE(a.is_archive());
  REQUIRE(a.as_path().filename() == "demo.zip");
  REQUIRE(a.entry_path().empty());

  const auto b = dirtoo::fs::Location::from_human("/tmp/demo.zip//archive:docs/a");
  REQUIRE(b.is_archive());
  REQUIRE(b.entry_path() == "docs/a");

  // Session/history must not collapse archive to the container path only.
  const auto url = b.as_url();
  const auto again = dirtoo::fs::Location::from_human(url);
  REQUIRE(again.is_archive());
  REQUIRE(again.as_path() == b.as_path());
  REQUIRE(again.entry_path() == b.entry_path());

  // Plain file path still works.
  const auto f = dirtoo::fs::Location::from_human("/tmp/plain");
  REQUIRE(f.is_file());
  REQUIRE_FALSE(f.is_archive());
}

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboard.hpp"

#include <catch2/catch_test_macros.hpp>

using dirtoo::app::ClipboardMode;
using dirtoo::app::parse_dirtoo_clipboard_text;
using dirtoo::app::parse_gnome_clipboard_text;

TEST_CASE("parse_dirtoo_clipboard_text copy", "[clipboard]")
{
  const auto p = parse_dirtoo_clipboard_text(QStringLiteral("copy\n/tmp/a\n/tmp/b\n"));
  REQUIRE(p.mode == ClipboardMode::Copy);
  REQUIRE(p.paths.size() == 2);
  REQUIRE(p.paths[0] == "/tmp/a");
  REQUIRE(p.paths[1] == "/tmp/b");
}

TEST_CASE("parse_dirtoo_clipboard_text cut", "[clipboard]")
{
  const auto p = parse_dirtoo_clipboard_text(QStringLiteral("cut\n/home/x\n"));
  REQUIRE(p.mode == ClipboardMode::Cut);
  REQUIRE(p.paths.size() == 1);
}

TEST_CASE("parse_gnome_clipboard_text", "[clipboard]")
{
  const auto p = parse_gnome_clipboard_text(
      QStringLiteral("copy\nfile:///tmp/hello%20world\nfile:///tmp/z\n"));
  REQUIRE(p.mode == ClipboardMode::Copy);
  REQUIRE(p.paths.size() == 2);
  REQUIRE(p.paths[0] == "/tmp/hello world");
}

TEST_CASE("parse empty clipboard text", "[clipboard]")
{
  const auto p = parse_dirtoo_clipboard_text(QString());
  REQUIRE(p.paths.empty());
}

TEST_CASE("parse_dirtoo_clipboard_text link", "[clipboard]")
{
  const auto p = parse_dirtoo_clipboard_text(QStringLiteral("link\n/tmp/a\n/tmp/b\n"));
  REQUIRE(p.mode == ClipboardMode::Link);
  REQUIRE(p.paths.size() == 2);
  REQUIRE(p.paths[0] == std::filesystem::path{"/tmp/a"});
}

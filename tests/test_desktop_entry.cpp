// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop_entry.hpp"

#include <catch2/catch_test_macros.hpp>

using dirtoo::app::parse_desktop_entry;

TEST_CASE("desktop entry keeps commas in Name and Exec", "[desktop]")
{
  // QSettings (IniFormat) reads both of these back as an empty string.
  const auto e = parse_desktop_entry(
      "[Desktop Entry]\nName=Foo, Bar\nExec=env A=1,2 foo %F\nIcon=x\nType=Application\n");
  REQUIRE(e);
  CHECK(e->name == "Foo, Bar");
  CHECK(e->exec == "env A=1,2 foo %F");
  CHECK(e->icon == "x");
}

TEST_CASE("desktop entry only reads the Desktop Entry group", "[desktop]")
{
  const auto e = parse_desktop_entry(
      "[Desktop Action new]\nName=New window\nExec=other\n\n"
      "[Desktop Entry]\nName=App\nExec=app\n"
      "[Desktop Action later]\nExec=ignored\n");
  REQUIRE(e);
  CHECK(e->name == "App");
  CHECK(e->exec == "app");
  CHECK_FALSE(parse_desktop_entry("Name=x\nExec=y\n"));  // no group at all
}

TEST_CASE("desktop entry picks the localised name", "[desktop]")
{
  const char* text =
      "[Desktop Entry]\nName=Files\nName[de]=Dateien\nName[de_AT]=Dateien (AT)\nExec=f\n";
  CHECK(parse_desktop_entry(text, QLocale("de_AT"))->name == "Dateien (AT)");
  CHECK(parse_desktop_entry(text, QLocale("de_DE"))->name == "Dateien");
  CHECK(parse_desktop_entry(text, QLocale("fr_FR"))->name == "Files");
}

TEST_CASE("desktop entry flags, mime types and escapes", "[desktop]")
{
  const auto e = parse_desktop_entry(
      "# comment\n[Desktop Entry]\n  Name = Spaced \nExec=a\\sb\nNoDisplay=true\n"
      "Hidden=false\nTerminal=TRUE\nPath=/opt/app\nTryExec=app\n"
      "MimeType=image/png;image/jpeg;\n");
  REQUIRE(e);
  CHECK(e->name == "Spaced");
  CHECK(e->exec == "a b");
  CHECK(e->no_display);
  CHECK_FALSE(e->hidden);
  CHECK(e->terminal);
  CHECK(e->path == "/opt/app");
  CHECK(e->try_exec == "app");
  REQUIRE(e->mime_types.size() == 2);
  CHECK(e->mime_types[0] == "image/png");
  CHECK(e->mime_types[1] == "image/jpeg");
}

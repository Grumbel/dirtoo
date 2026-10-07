// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/archive/archive_index.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

fs::path fresh_dir(const char* name)
{
  const auto dir = fs::temp_directory_path() / name;
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

/// Builds tars with arbitrary (hostile) entry names — libarchive's writer
/// does not sanitise them.
class TarBuilder {
public:
  explicit TarBuilder(const fs::path& out)
      : a_(archive_write_new())
  {
    archive_write_set_format_pax(a_);
    archive_write_open_filename(a_, out.c_str());
  }
  ~TarBuilder()
  {
    archive_write_close(a_);
    archive_write_free(a_);
  }

  void file(const std::string& name, const std::string& data)
  {
    archive_entry* e = archive_entry_new();
    archive_entry_set_pathname(e, name.c_str());
    archive_entry_set_filetype(e, AE_IFREG);
    archive_entry_set_perm(e, 0644);
    archive_entry_set_size(e, static_cast<la_int64_t>(data.size()));
    archive_write_header(a_, e);
    archive_write_data(a_, data.data(), data.size());
    archive_entry_free(e);
  }

  void hardlink(const std::string& name, const std::string& target)
  {
    archive_entry* e = archive_entry_new();
    archive_entry_set_pathname(e, name.c_str());
    archive_entry_set_hardlink(e, target.c_str());
    archive_entry_set_filetype(e, AE_IFREG);
    archive_entry_set_perm(e, 0644);
    archive_entry_set_size(e, 0);
    archive_write_header(a_, e);
    archive_entry_free(e);
  }

private:
  archive* a_;
};

std::string slurp(const fs::path& p)
{
  std::ifstream in(p);
  return std::string(std::istreambuf_iterator<char>(in), {});
}

} // namespace

TEST_CASE("extract_archive keeps absolute entry names inside dest", "[archive][safety]")
{
  const auto work = fresh_dir("dirtoo-test-extract-abs");
  const auto outside = work / "outside";
  fs::create_directories(outside);
  const auto victim = outside / "victim.txt";

  const auto tar = work / "evil.tar";
  {
    TarBuilder b(tar);
    b.file("ok.txt", "fine");
    b.file(victim.string(), "pwned");  // absolute path
  }

  const auto dest = work / "dest";
  const auto r = dirtoo::archive::extract_archive_libarchive(tar, dest);
  (void)r;  // may succeed (entry neutralised) or fail; it must not escape
  REQUIRE_FALSE(fs::exists(victim));
  fs::remove_all(work);
}

TEST_CASE("extract_archive refuses dot-dot entry names", "[archive][safety]")
{
  const auto work = fresh_dir("dirtoo-test-extract-dotdot");
  const auto tar = work / "evil.tar";
  {
    TarBuilder b(tar);
    b.file("../escaped.txt", "pwned");
  }
  const auto dest = work / "dest";
  const auto r = dirtoo::archive::extract_archive_libarchive(tar, dest);
  (void)r;
  REQUIRE_FALSE(fs::exists(work / "escaped.txt"));
  fs::remove_all(work);
}

TEST_CASE("extract_archive does not hardlink to files outside dest", "[archive][safety]")
{
  const auto work = fresh_dir("dirtoo-test-extract-hardlink");
  const auto secret = work / "secret.txt";
  {
    std::ofstream(secret) << "original";
  }
  const auto tar = work / "evil.tar";
  {
    TarBuilder b(tar);
    b.hardlink("link", secret.string());   // link to an absolute outside file
    b.file("link", "overwritten");         // then write "through" it
  }
  const auto dest = work / "dest";
  const auto r = dirtoo::archive::extract_archive_libarchive(tar, dest);
  (void)r;
  REQUIRE(slurp(secret) == "original");
  fs::remove_all(work);
}

TEST_CASE("extract_member rejects absolute member paths", "[archive][safety]")
{
  const auto work = fresh_dir("dirtoo-test-member-abs");
  const auto outside = work / "outside.txt";
  const auto tar = work / "a.tar";
  {
    TarBuilder b(tar);
    b.file("ok.txt", "fine");
  }
  const auto r = dirtoo::archive::extract_member_libarchive(tar, outside, work / "dest");
  REQUIRE_FALSE(r.has_value());
  REQUIRE_FALSE(fs::exists(outside));

  const auto r2 = dirtoo::archive::extract_member_libarchive(tar, "../x", work / "dest");
  REQUIRE_FALSE(r2.has_value());
  fs::remove_all(work);
}

TEST_CASE("extract_member extracts a nested member", "[archive]")
{
  const auto work = fresh_dir("dirtoo-test-member-ok");
  const auto tar = work / "a.tar";
  {
    TarBuilder b(tar);
    b.file("dir/inner.txt", "hello");
  }
  const auto r = dirtoo::archive::extract_member_libarchive(tar, "dir/inner.txt", work / "dest");
  REQUIRE(r.has_value());
  REQUIRE(slurp(*r) == "hello");
  fs::remove_all(work);
}

TEST_CASE("extract_archive extracts regular entries", "[archive]")
{
  const auto work = fresh_dir("dirtoo-test-extract-ok");
  const auto tar = work / "a.tar";
  {
    TarBuilder b(tar);
    b.file("a.txt", "A");
    b.file("sub/b.txt", "B");
  }
  const auto r = dirtoo::archive::extract_archive_libarchive(tar, work / "dest");
  REQUIRE(r.has_value());
  REQUIRE(slurp(work / "dest" / "a.txt") == "A");
  REQUIRE(slurp(work / "dest" / "sub" / "b.txt") == "B");
  fs::remove_all(work);
}

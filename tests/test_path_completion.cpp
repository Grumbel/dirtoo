// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "path_completion_worker.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QObject>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;
using dirtoo::app::PathCompletionWorker;

namespace {

fs::path make_tree(const char* tag)
{
  const auto dir = fs::temp_directory_path()
                   / (std::string("dirtoo-complete-") + tag + "-" + std::to_string(::getpid()));
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir / "alpha");
  fs::create_directories(dir / "alps");
  fs::create_directories(dir / "beta");
  std::ofstream(dir / "alfile") << "x";
  return dir;
}

struct Result {
  quint64 id = 0;
  QString longest;
  QStringList candidates;
  bool got = false;
};

Result run(PathCompletionWorker& w, quint64 id, const QString& text)
{
  Result r;
  QObject::connect(&w, &PathCompletionWorker::completions_ready, &w,
                   [&](quint64 i, const QString& l, const QStringList& c) {
                     r = {i, l, c, true};
                   },
                   Qt::DirectConnection);
  w.complete(id, text);
  return r;
}

} // namespace

TEST_CASE("path completion lists matching directories only", "[completion]")
{
  int argc = 0;
  QCoreApplication app(argc, nullptr);
  const auto dir = make_tree("basic");
  PathCompletionWorker w;

  const QString base = QString::fromStdString(dir.string());
  const auto r = run(w, 1, base + "/al");
  REQUIRE(r.got);
  CHECK(r.candidates.size() == 2);                     // alpha/, alps/ — not the file "alfile"
  CHECK(r.candidates.contains(base + "/alpha/"));
  CHECK(r.candidates.contains(base + "/alps/"));
  CHECK_FALSE(r.candidates.contains(base + "/alfile/"));
  fs::remove_all(dir);
}

TEST_CASE("path completion keeps the ~ form the user typed", "[completion]")
{
  int argc = 0;
  QCoreApplication app(argc, nullptr);
  const auto dir = make_tree("tilde");
  const std::string old_home = std::getenv("HOME") != nullptr ? std::getenv("HOME") : "";
  ::setenv("HOME", dir.c_str(), 1);

  PathCompletionWorker w;
  const auto r = run(w, 1, "~/al");
  if (old_home.empty()) {
    ::unsetenv("HOME");
  } else {
    ::setenv("HOME", old_home.c_str(), 1);
  }

  REQUIRE(r.got);
  // Absolute /tmp/… candidates would be filtered out by the QCompleter, which
  // compares against the typed "~/al".
  CHECK(r.candidates.contains("~/alpha/"));
  CHECK(r.candidates.contains("~/alps/"));
  fs::remove_all(dir);
}

TEST_CASE("path completion skips a scan that a newer request superseded", "[completion]")
{
  int argc = 0;
  QCoreApplication app(argc, nullptr);
  const auto dir = make_tree("stale");
  PathCompletionWorker w;

  w.supersede(7);  // the UI has already queued request 7
  const auto r = run(w, 3, QString::fromStdString(dir.string()) + "/al");
  REQUIRE(r.got);
  CHECK(r.candidates.isEmpty());
  CHECK(r.id == 3);

  const auto fresh = run(w, 7, QString::fromStdString(dir.string()) + "/al");
  CHECK(fresh.candidates.size() == 2);
  fs::remove_all(dir);
}

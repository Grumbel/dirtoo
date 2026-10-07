// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/watcher/directory_watcher.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QPointer>
#include <QThread>

#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;
using dirtoo::watcher::DirectoryWatcher;

namespace {

fs::path fresh_dir(const char* tag)
{
  const auto dir = fs::temp_directory_path()
                   / (std::string("dirtoo-watch-") + tag + "-" + std::to_string(::getpid()));
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir);
  return dir;
}

template <typename Pred>
bool pump_until(Pred done, int timeout_ms = 3000)
{
  QElapsedTimer t;
  t.start();
  while (!done() && t.elapsed() < timeout_ms) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    QThread::msleep(5);
  }
  return done();
}

} // namespace

TEST_CASE("DirectoryWatcher reports created and removed names", "[watcher]")
{
  int argc = 0;
  QCoreApplication app(argc, nullptr);
  const auto dir = fresh_dir("names");

  DirectoryWatcher w;
  w.set_location(dirtoo::fs::Location::from_path(dir));
  QStringList created;
  QStringList removed;
  QObject::connect(&w, &DirectoryWatcher::entries_changed, &w,
                   [&](const QStringList& c, const QStringList& r, const QStringList&) {
                     created << c;
                     removed << r;
                   });
  w.start();
  REQUIRE(pump_until([&] { return w.has_name_deltas(); }));

  const auto file = dir / "new.txt";
  std::ofstream(file) << "x";
  const QString qfile = QString::fromStdString(file.string());
  REQUIRE(pump_until([&] { return created.contains(qfile); }));

  fs::remove(file);
  REQUIRE(pump_until([&] { return removed.contains(qfile); }));
  fs::remove_all(dir);
}

TEST_CASE("DirectoryWatcher survives destruction while start() is in flight", "[watcher][safety]")
{
  int argc = 0;
  QCoreApplication app(argc, nullptr);
  const auto dir = fresh_dir("uaf");

  for (int i = 0; i < 50; ++i) {
    auto* w = new DirectoryWatcher;
    w->set_location(dirtoo::fs::Location::from_path(dir));
    w->start();   // setup runs on a pool thread and posts back
    delete w;     // ...which must not touch the deleted watcher
  }
  // Let any late posts from the pool threads run.
  QElapsedTimer t;
  t.start();
  while (t.elapsed() < 300) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  }
  fs::remove_all(dir);
  SUCCEED();
}

TEST_CASE("DirectoryWatcher does not accumulate duplicate modify events", "[watcher]")
{
  int argc = 0;
  QCoreApplication app(argc, nullptr);
  const auto dir = fresh_dir("dups");
  const auto file = dir / "busy.log";
  std::ofstream(file) << "x";

  DirectoryWatcher w;
  w.set_location(dirtoo::fs::Location::from_path(dir));
  QStringList modified;
  QObject::connect(&w, &DirectoryWatcher::entries_changed, &w,
                   [&](const QStringList&, const QStringList&, const QStringList& m) {
                     modified << m;
                   });
  w.start();
  REQUIRE(pump_until([&] { return w.has_name_deltas(); }));

  {
    std::ofstream out(file, std::ios::app);
    for (int i = 0; i < 500; ++i) {
      out << "line\n";
      out.flush();
    }
  }
  REQUIRE(pump_until([&] { return !modified.isEmpty(); }));
  pump_until([] { return false; }, 200);
  // 500 writes to one file must collapse to a handful of entries.
  REQUIRE(modified.size() < 20);
  fs::remove_all(dir);
}

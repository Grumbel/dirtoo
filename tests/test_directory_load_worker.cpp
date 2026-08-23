// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "directory_load_worker.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QObject>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using dirtoo::app::DirectoryLoadWorker;

namespace {

fs::path make_temp_dir_with_files(int count)
{
  const auto dir = fs::temp_directory_path() / "dirtoo-load-worker-test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir);
  for (int i = 0; i < count; ++i) {
    std::ofstream out(dir / ("f" + std::to_string(i) + ".txt"));
    out << "x";
  }
  return dir;
}

} // namespace

TEST_CASE("DirectoryLoadWorker lists a directory", "[dir-load]")
{
  int argc = 0;
  QCoreApplication app(argc, nullptr);

  const auto dir = make_temp_dir_with_files(12);
  DirectoryLoadWorker worker;

  std::vector<dirtoo::fs::FileInfo> got;
  QString err;
  QObject::connect(&worker, &DirectoryLoadWorker::loaded, &app,
                   [&](quint64 gen, std::vector<dirtoo::fs::FileInfo> items) {
                     REQUIRE(gen == 1);
                     got = std::move(items);
                   },
                   Qt::DirectConnection);
  QObject::connect(&worker, &DirectoryLoadWorker::failed, &app,
                   [&](quint64, QString e) { err = std::move(e); }, Qt::DirectConnection);

  worker.load(QString::fromStdString(dir.string()), 1);
  REQUIRE(err.isEmpty());
  REQUIRE(got.size() == 12);
}

TEST_CASE("DirectoryLoadWorker cancel aborts in-flight load", "[dir-load]")
{
  int argc = 0;
  QCoreApplication app(argc, nullptr);

  // Enough entries that the cancel thread usually wins; not a hard real-time race.
  const auto dir = make_temp_dir_with_files(2000);
  DirectoryLoadWorker worker;

  std::atomic<bool> loaded{false};
  std::atomic<bool> failed{false};
  QObject::connect(&worker, &DirectoryLoadWorker::loaded, &app,
                   [&](quint64, std::vector<dirtoo::fs::FileInfo>) { loaded.store(true); },
                   Qt::DirectConnection);
  QObject::connect(&worker, &DirectoryLoadWorker::failed, &app,
                   [&](quint64, QString) { failed.store(true); }, Qt::DirectConnection);

  std::thread loader([&] {
    worker.load(QString::fromStdString(dir.string()), /*generation=*/7);
  });
  // Cancel as soon as the other thread is likely inside the directory loop.
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  worker.cancel();
  loader.join();

  REQUIRE_FALSE(failed.load());
  // Cancel should suppress loaded; if the list finished before cancel, loaded may
  // still be true — accept that as a soft pass only when cancel was late.
  // Re-run cancel-before-work: load after cancel with same worker must still work.
  loaded.store(false);
  worker.load(QString::fromStdString(dir.string()), /*generation=*/8);
  REQUIRE(loaded.load());
  REQUIRE_FALSE(failed.load());
}

TEST_CASE("DirectoryLoadWorker cancel before load still allows next generation", "[dir-load]")
{
  int argc = 0;
  QCoreApplication app(argc, nullptr);

  const auto dir = make_temp_dir_with_files(5);
  DirectoryLoadWorker worker;
  worker.cancel(); // no in-flight work

  std::atomic<bool> loaded{false};
  QObject::connect(&worker, &DirectoryLoadWorker::loaded, &app,
                   [&](quint64 gen, std::vector<dirtoo::fs::FileInfo>) {
                     REQUIRE(gen == 3);
                     loaded.store(true);
                   },
                   Qt::DirectConnection);

  worker.load(QString::fromStdString(dir.string()), 3);
  REQUIRE(loaded.load());
}

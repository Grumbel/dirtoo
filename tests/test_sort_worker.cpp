// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sort_worker.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QMetaObject>

#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using dirtoo::app::SortWorker;

// MainWindow calls the worker by name: QMetaObject::invokeMethod(..., "sort_items", ...).
// A signature mismatch only shows up at run time (the call silently does not
// happen), so exercise exactly that call.
TEST_CASE("SortWorker is callable by name with the arguments MainWindow passes", "[sort][worker]")
{
  int argc = 0;
  QCoreApplication app(argc, nullptr);
  qRegisterMetaType<std::vector<dirtoo::fs::FileInfo>>("std::vector<dirtoo::fs::FileInfo>");
  qRegisterMetaType<dirtoo::collection::SortKey>("dirtoo::collection::SortKey");

  const auto dir = fs::temp_directory_path() / "dirtoo-test-sortworker";
  fs::remove_all(dir);
  fs::create_directories(dir);
  for (const char* n : {"b", "a", "c"}) {
    std::ofstream(dir / n) << n;
  }
  auto items = dirtoo::fs::list_directory(dirtoo::fs::Location::from_path(dir));

  SortWorker worker;
  quint64 got_gen = 0;
  std::vector<std::string> got;
  QObject::connect(&worker, &SortWorker::sorted, &worker,
                   [&](quint64 gen, std::vector<dirtoo::fs::FileInfo> sorted) {
                     got_gen = gen;
                     for (const auto& fi : sorted) {
                       got.push_back(fi.basename());
                     }
                   },
                   Qt::DirectConnection);

  const bool invoked = QMetaObject::invokeMethod(
      &worker, "sort_items", Qt::DirectConnection,
      Q_ARG(std::vector<dirtoo::fs::FileInfo>, items),
      Q_ARG(dirtoo::collection::SortKey, dirtoo::collection::SortKey::Name), Q_ARG(bool, true),
      Q_ARG(bool, true), Q_ARG(quint32, 1234u), Q_ARG(quint64, 42));
  REQUIRE(invoked);
  CHECK(got_gen == 42);
  CHECK(got == std::vector<std::string>{"a", "b", "c"});
  fs::remove_all(dir);
}

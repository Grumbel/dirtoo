// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "transfer_worker.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using dirtoo::app::ClipboardMode;
using dirtoo::app::TransferErrorAction;
using dirtoo::app::TransferRequest;
using dirtoo::app::TransferSummary;
using dirtoo::app::TransferWorker;

namespace {

struct Fixture {
  fs::path root;
  fs::path src;
  fs::path dst;

  explicit Fixture(const char* tag)
  {
    root = fs::temp_directory_path() / (std::string("dirtoo-transfer-") + tag + "-" +
                                        std::to_string(::getpid()));
    std::error_code ec;
    fs::remove_all(root, ec);
    src = root / "src";
    dst = root / "dst";
    fs::create_directories(src);
    fs::create_directories(dst);
  }
  ~Fixture()
  {
    std::error_code ec;
    fs::permissions(src, fs::perms::owner_all, ec);
    for (const auto& e : fs::directory_iterator(src, ec)) {
      fs::permissions(e.path(), fs::perms::owner_all, ec);
    }
    fs::remove_all(root, ec);
  }
  fs::path good(const std::string& name) const
  {
    const auto p = src / name;
    std::ofstream(p) << "data " << name;
    return p;
  }
  /// A file that cannot be read: copying it fails with "failed to open source".
  fs::path unreadable(const std::string& name) const
  {
    const auto p = good(name);
    fs::permissions(p, fs::perms::none);
    return p;
  }
};

/// Runs a transfer; `on_failure(source, remaining)` answers each item_failed.
TransferSummary run_transfer(const std::vector<fs::path>& sources, const fs::path& dst,
                             std::function<TransferErrorAction(const QString&, int)> on_failure,
                             int* failure_prompts = nullptr)
{
  qRegisterMetaType<TransferSummary>("dirtoo::app::TransferSummary");
  TransferWorker worker;
  TransferSummary summary;
  int prompts = 0;
  // DirectConnection: the slot runs inside the worker's emit, i.e. before it
  // starts waiting, so answering from here cannot deadlock.
  QObject::connect(&worker, &TransferWorker::item_failed, &worker,
                   [&](const QString& source, const QString&, int remaining) {
                     ++prompts;
                     worker.resolve_error(on_failure(source, remaining));
                   },
                   Qt::DirectConnection);
  QObject::connect(&worker, &TransferWorker::finished, &worker,
                   [&](TransferSummary s) { summary = std::move(s); }, Qt::DirectConnection);
  TransferRequest req;
  req.mode = ClipboardMode::Copy;
  req.destination_directory = dst;
  req.sources = sources;
  worker.run(req);
  if (failure_prompts != nullptr) {
    *failure_prompts = prompts;
  }
  return summary;
}

} // namespace

TEST_CASE("a failed item can be skipped and the transfer continues", "[transfer]")
{
  if (::geteuid() == 0) {
    SKIP("permissions are not enforced for root");
  }
  int argc = 0;
  QCoreApplication app(argc, nullptr);
  Fixture f("skip");
  const auto a = f.good("a.txt");
  const auto bad = f.unreadable("bad.txt");
  const auto c = f.good("c.txt");

  int prompts = 0;
  const auto s = run_transfer({a, bad, c}, f.dst,
                              [](const QString&, int remaining) {
                                CHECK(remaining == 1);  // c.txt is still to come
                                return TransferErrorAction::Skip;
                              },
                              &prompts);
  CHECK(prompts == 1);
  CHECK(s.error.isEmpty());
  CHECK(s.completed == 2);
  CHECK(s.failed == 1);
  REQUIRE(s.failures.size() == 1);
  CHECK(s.failures[0].startsWith("bad.txt:"));
  CHECK(fs::exists(f.dst / "a.txt"));
  CHECK_FALSE(fs::exists(f.dst / "bad.txt"));
  CHECK(fs::exists(f.dst / "c.txt"));  // the old behaviour stopped before this one
}

TEST_CASE("abort stops the transfer with the error", "[transfer]")
{
  if (::geteuid() == 0) {
    SKIP("permissions are not enforced for root");
  }
  int argc = 0;
  QCoreApplication app(argc, nullptr);
  Fixture f("abort");
  const auto bad = f.unreadable("bad.txt");
  const auto c = f.good("c.txt");

  const auto s = run_transfer({bad, c}, f.dst,
                              [](const QString&, int) { return TransferErrorAction::Abort; });
  CHECK_FALSE(s.error.isEmpty());
  CHECK(s.completed == 0);
  CHECK(s.failed == 0);
  CHECK_FALSE(fs::exists(f.dst / "c.txt"));
}

TEST_CASE("skip all asks only once", "[transfer]")
{
  if (::geteuid() == 0) {
    SKIP("permissions are not enforced for root");
  }
  int argc = 0;
  QCoreApplication app(argc, nullptr);
  Fixture f("skipall");
  const auto b1 = f.unreadable("b1.txt");
  const auto b2 = f.unreadable("b2.txt");
  const auto c = f.good("c.txt");

  int prompts = 0;
  const auto s = run_transfer({b1, b2, c}, f.dst,
                              [](const QString&, int) { return TransferErrorAction::SkipAll; },
                              &prompts);
  CHECK(prompts == 1);
  CHECK(s.failed == 2);
  CHECK(s.completed == 1);
  CHECK(s.error.isEmpty());
  CHECK(fs::exists(f.dst / "c.txt"));
}

TEST_CASE("retry runs the item again after the cause is fixed", "[transfer]")
{
  if (::geteuid() == 0) {
    SKIP("permissions are not enforced for root");
  }
  int argc = 0;
  QCoreApplication app(argc, nullptr);
  Fixture f("retry");
  const auto bad = f.unreadable("bad.txt");

  int prompts = 0;
  const auto s = run_transfer({bad}, f.dst,
                              [&](const QString& source, int) {
                                // The user fixes the permissions, then presses Retry.
                                fs::permissions(source.toStdString(),
                                                fs::perms::owner_read | fs::perms::owner_write);
                                return TransferErrorAction::Retry;
                              },
                              &prompts);
  CHECK(prompts == 1);
  CHECK(s.failed == 0);
  CHECK(s.completed == 1);
  CHECK(s.error.isEmpty());
  CHECK(fs::exists(f.dst / "bad.txt"));
}

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thread_util.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QThread>

using dirtoo::app::stop_thread;

namespace {

/// Blocks its thread for a while, like a stat() on a hung mount.
class Blocker : public QObject {
  Q_OBJECT
public slots:
  void block(int ms) { QThread::msleep(static_cast<unsigned long>(ms)); }
};

} // namespace

TEST_CASE("stop_thread stops an idle thread and leaves it to the caller", "[thread]")
{
  int argc = 0;
  QCoreApplication app(argc, nullptr);
  auto* thread = new QThread;
  thread->start();
  QElapsedTimer t;
  t.start();
  CHECK(stop_thread(thread, 2000));
  CHECK(t.elapsed() < 1000);
  CHECK_FALSE(thread->isRunning());
  delete thread;
}

TEST_CASE("stop_thread detaches a thread that is stuck instead of destroying it", "[thread]")
{
  int argc = 0;
  QCoreApplication app(argc, nullptr);

  auto* owner = new QObject;
  auto* thread = new QThread(owner);
  auto* blocker = new Blocker;
  blocker->moveToThread(thread);
  QObject::connect(thread, &QThread::finished, blocker, &QObject::deleteLater);
  thread->start();
  QMetaObject::invokeMethod(blocker, [blocker] { blocker->block(400); }, Qt::QueuedConnection);
  QThread::msleep(50);  // let it enter the blocking call

  QPointer<QThread> guard(thread);
  QElapsedTimer t;
  t.start();
  CHECK_FALSE(stop_thread(thread, 60));   // gave up waiting
  CHECK(t.elapsed() < 300);               // ...without waiting for the 400 ms block
  CHECK(thread->parent() == nullptr);     // no longer owned: deleting `owner` is now safe

  delete owner;                           // would abort if the thread were still its child
  CHECK(guard);                           // the thread object survived

  // Once the blocking call returns the thread finishes and deletes itself.
  QElapsedTimer wait;
  wait.start();
  while (guard && wait.elapsed() < 3000) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QThread::msleep(10);
  }
  CHECK_FALSE(guard);
}

#include "test_thread_util.moc"

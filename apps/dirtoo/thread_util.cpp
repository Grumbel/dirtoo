// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thread_util.hpp"

#include <QDebug>
#include <QObject>
#include <QThread>

namespace dirtoo::app {

bool stop_thread(QThread* thread, int grace_ms)
{
  if (thread == nullptr) {
    return true;
  }
  thread->quit();
  if (thread->wait(grace_ms)) {
    return true;
  }
  qWarning().noquote() << QStringLiteral("dirtoo: thread '%1' still busy after %2 ms; detaching")
                              .arg(thread->objectName())
                              .arg(grace_ms);
  // Outlive the owner; free the QThread once its blocking call returns.
  QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
  thread->setParent(nullptr);
  return false;
}

} // namespace dirtoo::app

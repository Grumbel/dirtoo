// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "async_io.hpp"

namespace dirtoo::app {

QThreadPool* io_thread_pool()
{
  // Intentionally leaked: a thread stuck in uninterruptible I/O on a hung mount
  // cannot be joined anyway, and a pool destructor would block process exit.
  static QThreadPool* pool = [] {
    auto* p = new QThreadPool;
    p->setObjectName(QStringLiteral("dirtoo-io"));
    p->setMaxThreadCount(16);
    return p;
  }();
  return pool;
}

} // namespace dirtoo::app

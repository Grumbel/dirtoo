// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QThreadPool>
#include <QtConcurrent>

#include <utility>

namespace dirtoo::app {

/// Thread pool for short blocking filesystem calls (stat, readlink, rename,
/// mkdir, …) issued on behalf of the GUI. Separate from the global pool so a
/// few calls hung on a dead network/USB mount (30 s+) do not starve unrelated
/// QtConcurrent work, and sized generously because these threads mostly sleep
/// in the kernel rather than burn CPU.
[[nodiscard]] QThreadPool* io_thread_pool();

/// Run @p work on io_thread_pool() and deliver its result to @p done on
/// @p context's thread. If @p context is destroyed first, @p done is dropped
/// (the work still runs to completion; its result is discarded).
///
/// This is the standard way for GUI code to touch the filesystem: never call
/// stat/exists/readlink/rename/remove directly on the GUI thread — paths may
/// live on a slow USB or network drive that takes tens of seconds to answer.
template <typename Work, typename Done>
void run_io(QObject* context, Work&& work, Done&& done)
{
  (void)QtConcurrent::run(io_thread_pool(), std::forward<Work>(work))
      .then(context, std::forward<Done>(done));
}

} // namespace dirtoo::app

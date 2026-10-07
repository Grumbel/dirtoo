// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QThread;

namespace dirtoo::app {

/// Stop a worker thread that runs an event loop.
///
/// Asks it to quit and waits up to @p grace_ms. A thread still blocked in I/O
/// on a hung mount cannot be interrupted, and destroying a running QThread
/// aborts the process, so after the grace period the thread is detached
/// (parent cleared) and deletes itself when it eventually finishes.
/// Returns true if the thread stopped in time (the caller still owns it and
/// may delete it), false if it was detached (the caller must not touch it
/// again).
bool stop_thread(QThread* thread, int grace_ms = 3000);

} // namespace dirtoo::app

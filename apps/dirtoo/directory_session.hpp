// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <QSet>
#include <QString>
#include <QtGlobal>

#include <optional>

namespace dirtoo::app {

/// Soft-reload and "new file" tracking for the open directory.
struct DirectorySession {
  bool soft_reload = false;
  QSet<QString> known_paths;
  fs::Location known_paths_location;
  fs::Location pending_archive_location;
  /// FilterWorker generation started by a soft reload: its result keeps the
  /// current row order instead of being sorted.
  std::optional<quint64> keep_order_filter_generation;

  void clear_known_paths()
  {
    known_paths.clear();
    known_paths_location = {};
  }
};

} // namespace dirtoo::app

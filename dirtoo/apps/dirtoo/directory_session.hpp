// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <QSet>
#include <QString>

namespace dirtoo::app {

/// Soft-reload and "new file" tracking for the open directory.
struct DirectorySession {
  bool soft_reload = false;
  QSet<QString> known_paths;
  fs::Location known_paths_location;
  fs::Location pending_archive_location;

  void clear_known_paths()
  {
    known_paths.clear();
    known_paths_location = {};
  }
};

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <QObject>

namespace dirtoo::watcher {

/// Watches a directory for changes. Phase 1 uses QFileSystemWatcher;
/// a dedicated inotify backend can replace it later for richer events.
class DirectoryWatcher : public QObject {
  Q_OBJECT

public:
  explicit DirectoryWatcher(QObject* parent = nullptr);
  ~DirectoryWatcher() override;

  void set_location(const fs::Location& location);
  [[nodiscard]] const fs::Location& location() const;

  void start();
  void stop();

signals:
  /// Emitted after initial scan and whenever the directory changes.
  void directory_changed();
  void message(const QString& text);

private:
  class Impl;
  Impl* impl_;
};

} // namespace dirtoo::watcher

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <QObject>
#include <QStringList>

#include <filesystem>
#include <vector>

namespace dirtoo::watcher {

/// Watches one or more filesystem paths for changes.
/// On Linux, uses inotify for per-name create/delete/modify when possible;
/// falls back to QFileSystemWatcher (directory/file changed only).
class DirectoryWatcher : public QObject {
  Q_OBJECT

public:
  explicit DirectoryWatcher(QObject* parent = nullptr);
  ~DirectoryWatcher() override;

  /// Logical location (used by callers); primary path watched is location.as_path()
  /// unless extra paths are set via set_extra_paths / set_watch_paths.
  void set_location(const fs::Location& location);
  [[nodiscard]] const fs::Location& location() const;

  /// Additional paths to watch alongside location().as_path() (e.g. archive extract root).
  void set_extra_paths(std::vector<std::filesystem::path> paths);

  /// Replace the full watch set (does not change logical location()).
  void set_watch_paths(std::vector<std::filesystem::path> paths);

  void start();
  void stop();

  /// True when the last start() successfully armed inotify for at least one directory.
  [[nodiscard]] bool has_name_deltas() const noexcept;

signals:
  /// Emitted whenever any watched directory or file changes (always; coarse signal).
  void directory_changed();

  /// Per-name deltas when inotify is active. Paths are absolute. May be empty
  /// lists for some categories. Coalesced briefly by the watcher.
  void entries_changed(const QStringList& created, const QStringList& removed,
                       const QStringList& modified);

  void message(const QString& text);

private:
  class Impl;
  Impl* impl_;
};

} // namespace dirtoo::watcher

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>

#include <filesystem>
#include <vector>

class QAction;

namespace dirtoo::app {

/// Background "is this path still there?" checks for menu entries (History,
/// Bookmarks, back/forward, Recently Opened).
///
/// Menus are built instantly with every entry enabled; track() then disables
/// an entry once all of its paths turn out to be missing (deleted, unplugged,
/// unmounted) and re-enables it if they come back. Results are cached, so a
/// reopened menu shows known-missing entries disabled right away. A path whose
/// check is still pending — e.g. stuck on a hung network mount — is not queued
/// again, so repeatedly opening a menu cannot pile up blocked I/O threads.
/// GUI thread only.
class PathAvailability : public QObject {
  Q_OBJECT

public:
  static PathAvailability& instance();

  /// Track @p action: disabled while every path in @p paths is missing.
  /// Empty @p paths (nothing on disk to check) leaves the action untouched.
  void track(QAction* action, const std::vector<std::filesystem::path>& paths);
  /// Convenience for locations: file:// and archive:// (the archive file) are
  /// checked; tag:// and set:// are virtual and left alone.
  void track(QAction* action, const fs::Location& location);

private:
  explicit PathAvailability(QObject* parent);

  enum class State { Present, Missing };

  void apply(QAction* action) const;
  void on_checked(const QString& key, bool exists);

  QHash<QString, State> known_;
  QSet<QString> in_flight_;
  /// Actions to re-evaluate when the check for a path returns.
  QHash<QString, QList<QPointer<QAction>>> waiting_;
};

} // namespace dirtoo::app

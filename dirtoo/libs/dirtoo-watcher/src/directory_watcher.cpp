// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/watcher/directory_watcher.hpp"

#include <QFileSystemWatcher>

namespace dirtoo::watcher {

class DirectoryWatcher::Impl {
public:
  fs::Location location;
  QFileSystemWatcher watcher;
  bool running = false;
};

DirectoryWatcher::DirectoryWatcher(QObject* parent)
    : QObject(parent)
    , impl_(new Impl)
{
  // Directory listings and archive extract trees.
  QObject::connect(&impl_->watcher, &QFileSystemWatcher::directoryChanged, this,
                   [this](const QString&) {
                     if (impl_->running) {
                       emit directory_changed();
                     }
                   });
  // Archive *files* (zip/tar) so replacing the archive on disk reloads the view.
  QObject::connect(&impl_->watcher, &QFileSystemWatcher::fileChanged, this,
                   [this](const QString&) {
                     if (impl_->running) {
                       emit directory_changed();
                     }
                   });
}

DirectoryWatcher::~DirectoryWatcher()
{
  stop();
  delete impl_;
}

void DirectoryWatcher::set_location(const fs::Location& location)
{
  stop();
  impl_->location = location;
}

const fs::Location& DirectoryWatcher::location() const
{
  return impl_->location;
}

void DirectoryWatcher::start()
{
  if (impl_->location.empty()) {
    emit message(QStringLiteral("DirectoryWatcher: empty location"));
    return;
  }
  const QString path = QString::fromStdString(impl_->location.as_path().string());
  if (!impl_->watcher.directories().contains(path) && !impl_->watcher.files().contains(path)) {
    if (!impl_->watcher.addPath(path)) {
      emit message(QStringLiteral("DirectoryWatcher: failed to watch %1").arg(path));
      return;
    }
  }
  impl_->running = true;
  // Do not emit directory_changed() here — navigation already loads explicitly.
  // Emitting would debounce another soft reload ~200ms after every open_location.
}

void DirectoryWatcher::stop()
{
  impl_->running = false;
  const auto dirs = impl_->watcher.directories();
  if (!dirs.isEmpty()) {
    impl_->watcher.removePaths(dirs);
  }
  const auto files = impl_->watcher.files();
  if (!files.isEmpty()) {
    impl_->watcher.removePaths(files);
  }
}

} // namespace dirtoo::watcher

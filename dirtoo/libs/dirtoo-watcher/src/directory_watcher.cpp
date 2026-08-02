// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/watcher/directory_watcher.hpp"

#include <QFileSystemWatcher>

#include <unordered_set>

namespace dirtoo::watcher {

class DirectoryWatcher::Impl {
public:
  fs::Location location;
  std::vector<std::filesystem::path> extra_paths;
  /// If non-empty, overrides location+extra for the actual watch set.
  std::vector<std::filesystem::path> override_paths;
  QFileSystemWatcher watcher;
  bool running = false;
};

DirectoryWatcher::DirectoryWatcher(QObject* parent)
    : QObject(parent)
    , impl_(new Impl)
{
  QObject::connect(&impl_->watcher, &QFileSystemWatcher::directoryChanged, this,
                   [this](const QString&) {
                     if (impl_->running) {
                       emit directory_changed();
                     }
                   });
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
  impl_->override_paths.clear();
}

const fs::Location& DirectoryWatcher::location() const
{
  return impl_->location;
}

void DirectoryWatcher::set_extra_paths(std::vector<std::filesystem::path> paths)
{
  const bool was_running = impl_->running;
  stop();
  impl_->extra_paths = std::move(paths);
  impl_->override_paths.clear();
  if (was_running) {
    start();
  }
}

void DirectoryWatcher::set_watch_paths(std::vector<std::filesystem::path> paths)
{
  const bool was_running = impl_->running;
  stop();
  impl_->override_paths = std::move(paths);
  if (was_running) {
    start();
  }
}

void DirectoryWatcher::start()
{
  std::vector<std::filesystem::path> paths;
  if (!impl_->override_paths.empty()) {
    paths = impl_->override_paths;
  } else {
    if (!impl_->location.empty()) {
      paths.push_back(impl_->location.as_path());
    }
    for (const auto& p : impl_->extra_paths) {
      paths.push_back(p);
    }
  }

  if (paths.empty()) {
    emit message(QStringLiteral("DirectoryWatcher: no paths to watch"));
    return;
  }

  std::unordered_set<std::string> seen;
  int added = 0;
  for (const auto& p : paths) {
    if (p.empty()) {
      continue;
    }
    const auto key = p.lexically_normal().string();
    if (!seen.insert(key).second) {
      continue;
    }
    const QString qpath = QString::fromStdString(key);
    if (impl_->watcher.directories().contains(qpath) || impl_->watcher.files().contains(qpath)) {
      ++added;
      continue;
    }
    if (impl_->watcher.addPath(qpath)) {
      ++added;
    } else {
      emit message(QStringLiteral("DirectoryWatcher: failed to watch %1").arg(qpath));
    }
  }
  if (added == 0) {
    emit message(QStringLiteral("DirectoryWatcher: failed to watch any path"));
    return;
  }
  impl_->running = true;
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

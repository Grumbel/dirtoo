// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/watcher/directory_watcher.hpp"

#include <QFileSystemWatcher>
#include <QSocketNotifier>
#include <QTimer>

#include <cerrno>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

#if defined(__linux__)
#  include <sys/inotify.h>
#  include <unistd.h>
#endif

namespace dirtoo::watcher {

class DirectoryWatcher::Impl {
public:
  fs::Location location;
  std::vector<std::filesystem::path> extra_paths;
  std::vector<std::filesystem::path> override_paths;
  QFileSystemWatcher qwatcher;
  bool running = false;
  bool name_deltas = false;

#if defined(__linux__)
  int inotify_fd = -1;
  QSocketNotifier* notifier = nullptr;
  std::unordered_map<int, std::filesystem::path> wd_to_dir;
  QStringList pending_created;
  QStringList pending_removed;
  QStringList pending_modified;
  QTimer* coalesce_timer = nullptr;
#endif
};

DirectoryWatcher::DirectoryWatcher(QObject* parent)
    : QObject(parent)
    , impl_(new Impl)
{
  QObject::connect(&impl_->qwatcher, &QFileSystemWatcher::directoryChanged, this,
                   [this](const QString&) {
                     if (impl_->running && !impl_->name_deltas) {
                       emit directory_changed();
                     }
                   });
  QObject::connect(&impl_->qwatcher, &QFileSystemWatcher::fileChanged, this,
                   [this](const QString&) {
                     if (impl_->running) {
                       // File watches (e.g. archive file) always use coarse signal.
                       emit directory_changed();
                     }
                   });

#if defined(__linux__)
  impl_->coalesce_timer = new QTimer(this);
  impl_->coalesce_timer->setSingleShot(true);
  impl_->coalesce_timer->setInterval(50);
  QObject::connect(impl_->coalesce_timer, &QTimer::timeout, this, [this] {
    if (!impl_->running) {
      return;
    }
    const QStringList c = impl_->pending_created;
    const QStringList r = impl_->pending_removed;
    const QStringList m = impl_->pending_modified;
    impl_->pending_created.clear();
    impl_->pending_removed.clear();
    impl_->pending_modified.clear();
    if (!c.isEmpty() || !r.isEmpty() || !m.isEmpty()) {
      emit entries_changed(c, r, m);
    }
  });
#endif
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

bool DirectoryWatcher::has_name_deltas() const noexcept
{
  return impl_->name_deltas;
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

  impl_->name_deltas = false;

#if defined(__linux__)
  // Prefer inotify for directories so we get per-name events.
  impl_->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (impl_->inotify_fd >= 0) {
    // IN_CLOSE_WRITE: editors often write via temp+rename or truncate+write;
    // close-after-write catches content changes that IN_MODIFY alone may miss or
    // coalesce poorly. IN_OPEN is intentionally omitted (too noisy).
    constexpr uint32_t kMask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO
                               | IN_MODIFY | IN_ATTRIB | IN_CLOSE_WRITE
                               | IN_DELETE_SELF | IN_MOVE_SELF;
    for (const auto& p : paths) {
      if (p.empty()) {
        continue;
      }
      std::error_code ec;
      if (!std::filesystem::is_directory(p, ec) || ec) {
        // Non-directories: fall through to QFileSystemWatcher file watch.
        continue;
      }
      const auto key = p.lexically_normal();
      const int wd = inotify_add_watch(impl_->inotify_fd, key.c_str(), kMask);
      if (wd >= 0) {
        impl_->wd_to_dir.emplace(wd, key);
        impl_->name_deltas = true;
      } else {
        emit message(QStringLiteral("DirectoryWatcher: inotify_add_watch failed for %1: %2")
                         .arg(QString::fromStdString(key.string()),
                              QString::fromLocal8Bit(std::strerror(errno))));
      }
    }
    if (impl_->name_deltas) {
      impl_->notifier = new QSocketNotifier(impl_->inotify_fd, QSocketNotifier::Read, this);
      QObject::connect(impl_->notifier, &QSocketNotifier::activated, this, [this](QSocketDescriptor) {
        if (!impl_->running || impl_->inotify_fd < 0) {
          return;
        }
        alignas(struct inotify_event) char buf[4096];
        for (;;) {
          const ssize_t n = ::read(impl_->inotify_fd, buf, sizeof(buf));
          if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              break;
            }
            break;
          }
          if (n == 0) {
            break;
          }
          ssize_t off = 0;
          while (off < n) {
            const auto* ev = reinterpret_cast<const struct inotify_event*>(buf + off);
            off += static_cast<ssize_t>(sizeof(struct inotify_event) + ev->len);
            const auto it = impl_->wd_to_dir.find(ev->wd);
            if (it == impl_->wd_to_dir.end()) {
              continue;
            }
            if ((ev->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) != 0) {
              // Directory itself went away — coarse reload.
              emit directory_changed();
              continue;
            }
            if (ev->len == 0 || ev->name[0] == '\0') {
              continue;
            }
            // Skip noisy internal names.
            if (ev->name[0] == '.' && (ev->name[1] == '\0' || (ev->name[1] == '.' && ev->name[2] == '\0'))) {
              continue;
            }
            const auto full = it->second / ev->name;
            const QString q = QString::fromStdString(full.string());
            if ((ev->mask & (IN_CREATE | IN_MOVED_TO)) != 0) {
              impl_->pending_created.push_back(q);
            }
            if ((ev->mask & (IN_DELETE | IN_MOVED_FROM)) != 0) {
              impl_->pending_removed.push_back(q);
            }
            if ((ev->mask & (IN_MODIFY | IN_ATTRIB | IN_CLOSE_WRITE)) != 0) {
              impl_->pending_modified.push_back(q);
            }
          }
        }
        if (impl_->coalesce_timer != nullptr
            && (!impl_->pending_created.isEmpty() || !impl_->pending_removed.isEmpty()
                || !impl_->pending_modified.isEmpty())) {
          impl_->coalesce_timer->start();
        }
      });
    } else {
      ::close(impl_->inotify_fd);
      impl_->inotify_fd = -1;
    }
  }
#endif

  // QFileSystemWatcher: always for files; for dirs when inotify is unavailable.
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
    std::error_code ec;
    const bool is_dir = std::filesystem::is_directory(p, ec) && !ec;
    if (is_dir && impl_->name_deltas) {
      // Already covered by inotify.
      ++added;
      continue;
    }
    const QString qpath = QString::fromStdString(key);
    if (impl_->qwatcher.directories().contains(qpath) || impl_->qwatcher.files().contains(qpath)) {
      ++added;
      continue;
    }
    if (impl_->qwatcher.addPath(qpath)) {
      ++added;
    } else {
      emit message(QStringLiteral("DirectoryWatcher: failed to watch %1").arg(qpath));
    }
  }
  if (added == 0 && !impl_->name_deltas) {
    emit message(QStringLiteral("DirectoryWatcher: failed to watch any path"));
    return;
  }
  impl_->running = true;
}

void DirectoryWatcher::stop()
{
  impl_->running = false;
  impl_->name_deltas = false;

#if defined(__linux__)
  if (impl_->coalesce_timer != nullptr) {
    impl_->coalesce_timer->stop();
  }
  impl_->pending_created.clear();
  impl_->pending_removed.clear();
  impl_->pending_modified.clear();
  if (impl_->notifier != nullptr) {
    delete impl_->notifier;
    impl_->notifier = nullptr;
  }
  if (impl_->inotify_fd >= 0) {
    for (const auto& [wd, dir] : impl_->wd_to_dir) {
      (void)dir;
      inotify_rm_watch(impl_->inotify_fd, wd);
    }
    impl_->wd_to_dir.clear();
    ::close(impl_->inotify_fd);
    impl_->inotify_fd = -1;
  }
#endif

  const auto dirs = impl_->qwatcher.directories();
  if (!dirs.isEmpty()) {
    impl_->qwatcher.removePaths(dirs);
  }
  const auto files = impl_->qwatcher.files();
  if (!files.isEmpty()) {
    impl_->qwatcher.removePaths(files);
  }
}

} // namespace dirtoo::watcher

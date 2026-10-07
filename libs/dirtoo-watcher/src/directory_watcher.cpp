// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/watcher/directory_watcher.hpp"

#include <QFileSystemWatcher>
#include <QSocketNotifier>
#include <QTimer>
#include <QCoreApplication>
#include <QMetaObject>
#include <QPointer>
#include <QtConcurrent>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#if defined(__linux__)
#  include <sys/inotify.h>
#  include <sys/statfs.h>
#  include <unistd.h>
#endif

namespace dirtoo::watcher {
namespace {

[[nodiscard]] bool is_remote_filesystem(const std::filesystem::path& path)
{
#if defined(__linux__)
  struct statfs st {};
  if (statfs(path.c_str(), &st) != 0) {
    return false;
  }
  // Common network / fuse magic numbers (linux/magic.h).
  switch (static_cast<unsigned long>(st.f_type)) {
  case 0x6969:      // NFS_SUPER_MAGIC
  case 0xFF534D42:  // CIFS_MAGIC_NUMBER
  case 0xFE534D42:  // SMB2_SUPER_MAGIC
  case 0x65735546:  // FUSE_SUPER_MAGIC
  case 0x65735543:  // FUSECTL
  case 0x6B414653:  // AFS
  case 0x517B:      // SMB_SUPER_MAGIC (older)
  case 0x564C:      // NCP_SUPER_MAGIC
  case 0x01021997:  // V9FS
    return true;
  default:
    return false;
  }
#else
  (void)path;
  return false;
#endif
}

/// Cheap content fingerprint for poll fallback (worker thread only).
[[nodiscard]] std::string directory_fingerprint(const std::filesystem::path& dir)
{
  std::ostringstream oss;
  std::error_code ec;
  const auto mtime = std::filesystem::last_write_time(dir, ec);
  if (!ec) {
    oss << mtime.time_since_epoch().count();
  } else {
    oss << "x";
  }
  oss << '|';
  auto opts = std::filesystem::directory_options::skip_permission_denied;
  std::size_t count = 0;
  std::uint64_t name_hash = 14695981039346656037ull; // FNV-1a offset
  for (const auto& entry : std::filesystem::directory_iterator(dir, opts, ec)) {
    if (ec) {
      break;
    }
    ++count;
    const auto name = entry.path().filename().string();
    for (unsigned char c : name) {
      name_hash ^= c;
      name_hash *= 1099511628211ull;
    }
    name_hash ^= static_cast<std::uint64_t>(name.size());
    // Cap work on huge directories — still detect create/delete via count.
    if (count >= 4096) {
      oss << count << "|trunc|" << name_hash;
      return oss.str();
    }
  }
  oss << count << '|' << name_hash;
  return oss.str();
}

[[nodiscard]] std::string paths_fingerprint(const std::vector<std::filesystem::path>& paths)
{
  std::ostringstream oss;
  for (const auto& p : paths) {
    if (p.empty()) {
      continue;
    }
    std::error_code ec;
    if (std::filesystem::is_directory(p, ec) && !ec) {
      oss << p.string() << '=' << directory_fingerprint(p) << ';';
    } else {
      const auto mtime = std::filesystem::last_write_time(p, ec);
      oss << p.string() << '=';
      if (!ec) {
        oss << mtime.time_since_epoch().count();
      } else {
        oss << "missing";
      }
      oss << ';';
    }
  }
  return oss.str();
}

} // namespace

class DirectoryWatcher::Impl {
public:
  fs::Location location;
  std::vector<std::filesystem::path> extra_paths;
  std::vector<std::filesystem::path> override_paths;
  QFileSystemWatcher qwatcher;
  bool running = false;
  bool name_deltas = false;
  /// Bumped on stop/start so in-flight async watch setup is ignored.
  std::uint64_t start_generation = 0;
  /// Paths we may poll (directories). Used when inotify is unreliable (NFS/SMB/…)
  /// or when neither inotify nor QFileSystemWatcher armed successfully.
  std::vector<std::filesystem::path> poll_paths;
  QTimer* poll_timer = nullptr;
  std::string last_fingerprint;
  bool poll_in_flight = false;
  bool poll_enabled = false;

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

  impl_->poll_timer = new QTimer(this);
  impl_->poll_timer->setInterval(2500);
  QObject::connect(impl_->poll_timer, &QTimer::timeout, this, [this] {
    if (!impl_->running || !impl_->poll_enabled || impl_->poll_in_flight) {
      return;
    }
    if (impl_->poll_paths.empty()) {
      return;
    }
    impl_->poll_in_flight = true;
    const std::uint64_t gen = impl_->start_generation;
    const auto paths = impl_->poll_paths;
    // Pool threads never touch `this`: they post to the (always alive)
    // application object and the GUI-thread continuation re-checks the guard.
    QPointer<DirectoryWatcher> guard(this);
    (void)QtConcurrent::run([guard, gen, paths]() {
      const std::string fp = paths_fingerprint(paths);
      QMetaObject::invokeMethod(
          QCoreApplication::instance(),
          [guard, gen, fp]() {
            if (!guard) {
              return;
            }
            auto* self = guard.data();
            auto* impl_ = self->impl_;
            impl_->poll_in_flight = false;
            if (!impl_->running || gen != impl_->start_generation || !impl_->poll_enabled) {
              return;
            }
            if (impl_->last_fingerprint.empty()) {
              impl_->last_fingerprint = fp;
              return;
            }
            if (fp != impl_->last_fingerprint) {
              impl_->last_fingerprint = fp;
              emit self->directory_changed();
            }
          },
          Qt::QueuedConnection);
    });
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

  // Mark running before the worker returns so stop() can cancel via generation.
  ++impl_->start_generation;
  const std::uint64_t gen = impl_->start_generation;
  impl_->running = true;
  impl_->name_deltas = false;
  impl_->poll_enabled = false;
  impl_->poll_in_flight = false;
  impl_->last_fingerprint.clear();
  impl_->poll_paths = paths;
  if (impl_->poll_timer != nullptr) {
    impl_->poll_timer->stop();
  }

  // is_directory / inotify_add_watch / QFileSystemWatcher::addPath can all
  // block for a long time on hung network mounts — never do that on the GUI thread.
  QPointer<DirectoryWatcher> guard(this);
  (void)QtConcurrent::run([guard, paths, gen]() {
    struct Result {
      bool name_deltas = false;
      int inotify_fd = -1;
      std::vector<std::pair<int, std::filesystem::path>> inotify_watches;
      std::vector<std::filesystem::path> qwatcher_paths;
      QStringList messages;
    } result;

#if defined(__linux__)
    result.inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (result.inotify_fd >= 0) {
      constexpr uint32_t kMask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO
                                 | IN_MODIFY | IN_ATTRIB | IN_CLOSE_WRITE
                                 | IN_DELETE_SELF | IN_MOVE_SELF;
      for (const auto& p : paths) {
        if (p.empty()) {
          continue;
        }
        std::error_code ec;
        if (!std::filesystem::is_directory(p, ec) || ec) {
          continue;
        }
        const auto key = p.lexically_normal();
        const int wd = inotify_add_watch(result.inotify_fd, key.c_str(), kMask);
        if (wd >= 0) {
          result.inotify_watches.emplace_back(wd, key);
          result.name_deltas = true;
        } else {
          result.messages << QStringLiteral("DirectoryWatcher: inotify_add_watch failed for %1: %2")
                                 .arg(QString::fromStdString(key.string()),
                                      QString::fromLocal8Bit(std::strerror(errno)));
        }
      }
      if (!result.name_deltas) {
        ::close(result.inotify_fd);
        result.inotify_fd = -1;
      }
    }
#endif

    std::unordered_set<std::string> seen;
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
      if (is_dir && result.name_deltas) {
        continue; // covered by inotify
      }
      result.qwatcher_paths.push_back(p.lexically_normal());
    }

    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [guard, gen, result = std::move(result)]() mutable {
          if (!guard || gen != guard->impl_->start_generation) {
            // Superseded by stop/start — drop any fd we opened.
#if defined(__linux__)
            if (result.inotify_fd >= 0) {
              ::close(result.inotify_fd);
            }
#endif
            return;
          }
          DirectoryWatcher* const self = guard.data();
          auto* const impl_ = self->impl_;
          for (const QString& m : result.messages) {
            emit self->message(m);
          }
#if defined(__linux__)
          // Clear any previous inotify state (stop() should have, but be safe).
          if (impl_->notifier != nullptr) {
            impl_->notifier->setEnabled(false);
            impl_->notifier->deleteLater();
            impl_->notifier = nullptr;
          }
          if (impl_->inotify_fd >= 0) {
            ::close(impl_->inotify_fd);
            impl_->inotify_fd = -1;
          }
          impl_->wd_to_dir.clear();
          impl_->inotify_fd = result.inotify_fd;
          impl_->name_deltas = result.name_deltas;
          for (const auto& [wd, key] : result.inotify_watches) {
            impl_->wd_to_dir.emplace(wd, key);
          }
          if (impl_->inotify_fd >= 0 && impl_->name_deltas) {
            impl_->notifier = new QSocketNotifier(impl_->inotify_fd, QSocketNotifier::Read, self);
            QObject::connect(impl_->notifier, &QSocketNotifier::activated, self,
                             [self, impl_](QSocketDescriptor) {
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
                  // Overflow (wd == -1) means events were dropped, and an
                  // unmount (USB pulled) invalidates the watch: only a full
                  // reload is correct in both cases.
                  if ((ev->mask & (IN_Q_OVERFLOW | IN_UNMOUNT)) != 0) {
                    emit self->directory_changed();
                    continue;
                  }
                  const auto it = impl_->wd_to_dir.find(ev->wd);
                  if (it == impl_->wd_to_dir.end()) {
                    continue;
                  }
                  if ((ev->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) != 0) {
                    emit self->directory_changed();
                    continue;
                  }
                  if (ev->len == 0 || ev->name[0] == '\0') {
                    continue;
                  }
                  if (ev->name[0] == '.'
                      && (ev->name[1] == '\0'
                          || (ev->name[1] == '.' && ev->name[2] == '\0'))) {
                    continue;
                  }
                  const auto full = it->second / ev->name;
                  const QString q = QString::fromStdString(full.string());
                  // A busy file raises an event per write(); keep one entry per
                  // run so the pending lists stay small.
                  auto note = [&q](QStringList& list) {
                    if (list.isEmpty() || list.last() != q) {
                      list.push_back(q);
                    }
                  };
                  if ((ev->mask & (IN_CREATE | IN_MOVED_TO)) != 0) {
                    note(impl_->pending_created);
                  }
                  if ((ev->mask & (IN_DELETE | IN_MOVED_FROM)) != 0) {
                    note(impl_->pending_removed);
                  }
                  if ((ev->mask & (IN_MODIFY | IN_ATTRIB | IN_CLOSE_WRITE)) != 0) {
                    note(impl_->pending_modified);
                  }
                }
              }
              if (impl_->coalesce_timer != nullptr
                  && !impl_->coalesce_timer->isActive()
                  && (!impl_->pending_created.isEmpty() || !impl_->pending_removed.isEmpty()
                      || !impl_->pending_modified.isEmpty())) {
                // Throttle, not debounce: restarting on every batch would
                // starve updates for as long as a file keeps being written.
                impl_->coalesce_timer->start();
              }
            });
          }
#endif
          int added = 0;
          for (const auto& p : result.qwatcher_paths) {
            const QString qpath = QString::fromStdString(p.string());
            if (impl_->qwatcher.directories().contains(qpath)
                || impl_->qwatcher.files().contains(qpath)) {
              ++added;
              continue;
            }
            if (impl_->qwatcher.addPath(qpath)) {
              ++added;
            } else {
              emit self->message(QStringLiteral("DirectoryWatcher: failed to watch %1").arg(qpath));
            }
          }
          if (impl_->name_deltas) {
            added += static_cast<int>(impl_->wd_to_dir.size());
          }
          if (added == 0 && !impl_->name_deltas) {
            emit self->message(QStringLiteral("DirectoryWatcher: failed to watch any path"));
          }

          // Poll fallback: network mounts often claim inotify success but miss
          // events; also enable when no native watch could be armed at all.
          bool remote = false;
          for (const auto& p : impl_->poll_paths) {
            if (!p.empty() && is_remote_filesystem(p)) {
              remote = true;
              break;
            }
          }
          // Prefer poll on remote even when inotify/qwatcher reported success.
          if (remote || (added == 0 && !impl_->name_deltas)) {
            impl_->poll_enabled = true;
            // Seed fingerprint asynchronously so the first tick only compares.
            const auto seed_paths = impl_->poll_paths;
            const std::uint64_t seed_gen = gen;
            (void)QtConcurrent::run([guard, seed_gen, seed_paths]() {
              const std::string fp = paths_fingerprint(seed_paths);
              QMetaObject::invokeMethod(
                  QCoreApplication::instance(),
                  [guard, seed_gen, fp]() {
                    if (!guard) {
                      return;
                    }
                    auto* const impl_ = guard->impl_;
                    if (!impl_->running || seed_gen != impl_->start_generation) {
                      return;
                    }
                    impl_->last_fingerprint = fp;
                  },
                  Qt::QueuedConnection);
            });
            if (impl_->poll_timer != nullptr) {
              impl_->poll_timer->start();
            }
            if (remote) {
              emit self->message(QStringLiteral(
                  "DirectoryWatcher: network filesystem detected — enabling poll fallback"));
            } else {
              emit self->message(QStringLiteral(
                  "DirectoryWatcher: no native watches — enabling poll fallback"));
            }
          }
        },
        Qt::QueuedConnection);
  });
}

void DirectoryWatcher::stop()
{
  ++impl_->start_generation;
  impl_->running = false;
  impl_->name_deltas = false;
  impl_->poll_enabled = false;
  impl_->poll_in_flight = false;
  impl_->last_fingerprint.clear();
  if (impl_->poll_timer != nullptr) {
    impl_->poll_timer->stop();
  }

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

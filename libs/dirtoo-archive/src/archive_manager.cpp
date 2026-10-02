// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/archive/archive_manager.hpp"
#include "dirtoo/archive/archive_index.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QStandardPaths>

#include <fstream>
#include <thread>

namespace dirtoo::archive {
namespace {

std::string key_of(const fs::Location& loc)
{
  return loc.as_path().string();
}

} // namespace

ArchiveManager::ArchiveManager(QObject* parent)
    : ArchiveManager({}, parent)
{
}

ArchiveManager::ArchiveManager(std::filesystem::path cache_root, QObject* parent)
    : QObject(parent)
    , cache_root_(std::move(cache_root))
{
  if (cache_root_.empty()) {
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    cache_root_ = std::filesystem::path{base.toStdString()} / "archives";
  }
  std::error_code ec;
  std::filesystem::create_directories(cache_root_, ec);
}

std::filesystem::path ArchiveManager::cache_dir_for(const std::filesystem::path& cache_root,
                                                    const std::filesystem::path& archive_file)
{
  const QByteArray hash =
      QCryptographicHash::hash(QByteArray::fromStdString(archive_file.string()),
                               QCryptographicHash::Sha1)
          .toHex();
  QFileInfo fi(QString::fromStdString(archive_file.string()));
  // mtime + size so a replaced archive (same path) does not reuse a stale extract.
  const QString stamp = QString::number(fi.lastModified().toSecsSinceEpoch()) + QLatin1Char('-')
                        + QString::number(fi.size());
  return cache_root / (hash + "-" + stamp.toUtf8()).toStdString();
}

std::optional<std::filesystem::path>
ArchiveManager::extracted_root(const fs::Location& archive_location) const
{
  const auto it = entries_.find(key_of(archive_location));
  if (it == entries_.end() || it->second.status != ExtractStatus::Ready) {
    return std::nullopt;
  }
  return it->second.cache_dir;
}

std::optional<std::filesystem::path>
ArchiveManager::resolved_directory(const fs::Location& location) const
{
  if (!location.is_archive()) {
    return location.as_path();
  }
  const auto root = extracted_root(location);
  if (!root) {
    return std::nullopt;
  }
  if (location.entry_path().empty()) {
    return *root;
  }
  return *root / location.entry_path();
}

ExtractStatus ArchiveManager::status(const fs::Location& archive_location) const
{
  const auto it = entries_.find(key_of(archive_location));
  if (it == entries_.end()) {
    return ExtractStatus::Idle;
  }
  return it->second.status;
}

QString ArchiveManager::last_error(const fs::Location& archive_location) const
{
  const auto it = entries_.find(key_of(archive_location));
  if (it == entries_.end()) {
    return {};
  }
  return it->second.error;
}

void ArchiveManager::open(const fs::Location& archive_location)
{
  const fs::Location archive_root = fs::Location::from_archive(archive_location.as_path(), {});
  auto& entry = entries_[key_of(archive_root)];
  if (entry.status == ExtractStatus::Working) {
    return;
  }
  entry.status = ExtractStatus::Working;
  entry.error.clear();
  emit extraction_started(archive_root);

  // Everything that touches the filesystem runs on the worker: the cache
  // stamp stats the archive (which may sit on a slow USB/network drive), plus
  // the marker check, cache reset and libarchive extract. A Ready entry is
  // re-validated the same way, so a replaced archive is re-extracted.
  const auto archive_path = archive_root.as_path();
  const auto cache_root = cache_root_;
  QPointer<ArchiveManager> guard(this);
  std::thread([guard, archive_root, archive_path, cache_root]() {
    const auto cache_dir = cache_dir_for(cache_root, archive_path);
    const auto marker = cache_dir / ".dirtoo-extracted";
    QString err;
    std::error_code ec;
    if (!std::filesystem::exists(marker, ec)) {
      std::filesystem::remove_all(cache_dir, ec);
      std::filesystem::create_directories(cache_dir, ec);
      if (ec) {
        err = QString::fromStdString(ec.message());
      } else if (const auto ok = extract_archive_libarchive(archive_path, cache_dir); !ok) {
        err = QString::fromStdString(ok.error());
      } else {
        std::ofstream out(marker);
        out << archive_path.string() << '\n';
        out << cache_dir.filename().string() << '\n'; // stamp segment (mtime-size)
      }
    }
    // Post to the application object (always alive) and re-check the guard on
    // the GUI thread: the manager may have been destroyed while extracting.
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [guard, archive_root, cache_dir, err]() {
          if (!guard) {
            return;
          }
          if (err.isEmpty()) {
            guard->finish_ok(archive_root, cache_dir);
          } else {
            guard->finish_fail(archive_root, err);
          }
        },
        Qt::QueuedConnection);
  }).detach();
}

void ArchiveManager::finish_ok(const fs::Location& archive_location,
                               const std::filesystem::path& cache_dir)
{
  auto it = entries_.find(key_of(archive_location));
  if (it == entries_.end()) {
    return;
  }
  it->second.status = ExtractStatus::Ready;
  it->second.cache_dir = cache_dir;
  emit extraction_ready(archive_location, cache_dir);
}

void ArchiveManager::finish_fail(const fs::Location& archive_location, const QString& message)
{
  auto it = entries_.find(key_of(archive_location));
  if (it == entries_.end()) {
    return;
  }
  it->second.status = ExtractStatus::Failed;
  it->second.error = message;
  emit extraction_failed(archive_location, message);
}

} // namespace dirtoo::archive

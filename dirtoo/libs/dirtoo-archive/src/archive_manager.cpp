// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/archive/archive_manager.hpp"
#include "dirtoo/archive/archive_index.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <fstream>

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

std::filesystem::path ArchiveManager::cache_dir_for(const std::filesystem::path& archive_file) const
{
  const QByteArray hash =
      QCryptographicHash::hash(QByteArray::fromStdString(archive_file.string()),
                               QCryptographicHash::Sha1)
          .toHex();
  QFileInfo fi(QString::fromStdString(archive_file.string()));
  // mtime + size so a replaced archive (same path) does not reuse a stale extract.
  const QString stamp = QString::number(fi.lastModified().toSecsSinceEpoch()) + QLatin1Char('-')
                        + QString::number(fi.size());
  return cache_root_ / (hash + "-" + stamp.toUtf8()).toStdString();
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
  fs::Location archive_root = archive_location;
  if (archive_location.is_archive()) {
    archive_root = fs::Location::from_archive(archive_location.as_path(), {});
  } else {
    archive_root = fs::Location::from_archive(archive_location.as_path(), {});
  }

  const std::string key = key_of(archive_root);
  auto& entry = entries_[key];
  const auto expected_dir = cache_dir_for(archive_root.as_path());

  // In-session: Ready must still match current archive mtime/size stamp.
  if (entry.status == ExtractStatus::Ready) {
    if (entry.cache_dir == expected_dir
        && std::filesystem::exists(entry.cache_dir / ".dirtoo-extracted")) {
      emit extraction_ready(archive_root, entry.cache_dir);
      return;
    }
    // Archive file changed (or cache wiped) — drop stale Ready state.
    entry.status = ExtractStatus::Idle;
    entry.cache_dir.clear();
    entry.error.clear();
  }
  if (entry.status == ExtractStatus::Working) {
    return;
  }

  entry.cache_dir = expected_dir;
  const auto marker = entry.cache_dir / ".dirtoo-extracted";
  if (std::filesystem::is_directory(entry.cache_dir) && std::filesystem::exists(marker)) {
    entry.status = ExtractStatus::Ready;
    emit extraction_ready(archive_root, entry.cache_dir);
    return;
  }

  start_extract(archive_root, entry);
}

void ArchiveManager::start_extract(const fs::Location& archive_location, Entry& entry)
{
  std::error_code ec;
  std::filesystem::remove_all(entry.cache_dir, ec);
  std::filesystem::create_directories(entry.cache_dir, ec);
  if (ec) {
    finish_fail(archive_location, QString::fromStdString(ec.message()));
    return;
  }

  entry.status = ExtractStatus::Working;
  entry.error.clear();
  emit extraction_started(archive_location);

  const QString archive = QString::fromStdString(archive_location.as_path().string());
  const QString out_dir = QString::fromStdString(entry.cache_dir.string());

  // Prefer in-process libarchive extract (no PATH dependency, correct sizes already used for TOC).
  if (libarchive_available()) {
    auto ok = extract_archive_libarchive(archive_location.as_path(), entry.cache_dir);
    if (ok) {
      finish_ok(archive_location);
      return;
    }
    // Fall through to external tools if libarchive extract fails.
    entry.error = QString::fromStdString(ok.error());
  }

  auto* process = new QProcess(this);
  entry.process = process;

  // Prefer bsdtar (libarchive CLI), then tar, then unzip for .zip.
  QString program;
  QStringList args;
  const QString lower = archive.toLower();

  if (!QStandardPaths::findExecutable(QStringLiteral("bsdtar")).isEmpty()) {
    program = QStringLiteral("bsdtar");
    args << QStringLiteral("-xf") << archive << QStringLiteral("-C") << out_dir;
  } else if (lower.endsWith(QLatin1String(".zip"))
             && !QStandardPaths::findExecutable(QStringLiteral("unzip")).isEmpty()) {
    program = QStringLiteral("unzip");
    args << QStringLiteral("-q") << archive << QStringLiteral("-d") << out_dir;
  } else if (!QStandardPaths::findExecutable(QStringLiteral("tar")).isEmpty()) {
    program = QStringLiteral("tar");
    args << QStringLiteral("-xf") << archive << QStringLiteral("-C") << out_dir;
  } else if (!QStandardPaths::findExecutable(QStringLiteral("7z")).isEmpty()) {
    program = QStringLiteral("7z");
    args << QStringLiteral("x") << QStringLiteral("-y") << QStringLiteral("-o") + out_dir << archive;
  } else {
    finish_fail(archive_location,
                QStringLiteral("No archive tool found (bsdtar, tar, unzip, or 7z)"));
    process->deleteLater();
    entry.process = nullptr;
    return;
  }

  connect(process, &QProcess::finished, this,
          [this, archive_location, process](int code, QProcess::ExitStatus status) {
            auto it = entries_.find(key_of(archive_location));
            if (it == entries_.end()) {
              process->deleteLater();
              return;
            }
            it->second.process = nullptr;
            if (status != QProcess::NormalExit || code != 0) {
              const QString err = QString::fromLocal8Bit(process->readAllStandardError());
              finish_fail(archive_location,
                          err.isEmpty() ? QStringLiteral("Extractor exited with code %1").arg(code)
                                        : err);
            } else {
              finish_ok(archive_location);
            }
            process->deleteLater();
          });

  process->start(program, args);
  if (!process->waitForStarted(3000)) {
    finish_fail(archive_location, QStringLiteral("Failed to start %1").arg(program));
    process->deleteLater();
    entry.process = nullptr;
  }
}

void ArchiveManager::finish_ok(const fs::Location& archive_location)
{
  auto it = entries_.find(key_of(archive_location));
  if (it == entries_.end()) {
    return;
  }
  it->second.status = ExtractStatus::Ready;
  std::ofstream marker(it->second.cache_dir / ".dirtoo-extracted");
  marker << archive_location.as_path().string() << '\n';
  marker << it->second.cache_dir.filename().string() << '\n'; // stamp segment (mtime-size)
  emit extraction_ready(archive_location, it->second.cache_dir);
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

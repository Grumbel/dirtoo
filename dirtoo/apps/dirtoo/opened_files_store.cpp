// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "opened_files_store.hpp"

#include "open_history.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

#include <fstream>

namespace dirtoo::app {
namespace {

constexpr int kMaxEntries = 50000;

} // namespace

OpenedFilesStore::OpenedFilesStore(std::filesystem::path file, QObject* parent)
    : QObject(parent), path_(std::move(file))
{
  load();
}

std::filesystem::path OpenedFilesStore::default_path()
{
  const QString base =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(base);
  return std::filesystem::path(
      QDir(base).filePath(QStringLiteral("opened_files.txt")).toStdString());
}

QString OpenedFilesStore::normalize(const QString& path)
{
  if (path.isEmpty()) {
    return {};
  }
  const QFileInfo fi(path);
  if (fi.exists()) {
    return fi.absoluteFilePath();
  }
  // Still normalize as path text so marks survive temporary missing files.
  const QString cleaned = QDir::cleanPath(path);
  if (QDir::isAbsolutePath(cleaned)) {
    return cleaned;
  }
  return QFileInfo(cleaned).absoluteFilePath();
}

QString OpenedFilesStore::normalize(const std::filesystem::path& path)
{
  return normalize(QString::fromStdString(path.string()));
}

void OpenedFilesStore::load()
{
  opened_.clear();
  loaded_ = true;
  std::ifstream in(path_);
  if (!in) {
    return;
  }
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    opened_.insert(line);
    if (static_cast<int>(opened_.size()) >= kMaxEntries) {
      break;
    }
  }
}

void OpenedFilesStore::save() const
{
  const auto parent = path_.parent_path();
  if (!parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
  }
  const std::filesystem::path tmp = path_.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) {
      return;
    }
    out << "# dirtoo opened-files marks (one absolute path per line)\n";
    for (const auto& p : opened_) {
      out << p << '\n';
    }
  }
  std::error_code ec;
  std::filesystem::rename(tmp, path_, ec);
  if (ec) {
    std::filesystem::remove(path_, ec);
    std::filesystem::rename(tmp, path_, ec);
  }
}

bool OpenedFilesStore::is_opened(const QString& path) const
{
  const QString n = normalize(path);
  if (n.isEmpty()) {
    return false;
  }
  return opened_.find(n.toStdString()) != opened_.end();
}

bool OpenedFilesStore::is_opened(const std::filesystem::path& path) const
{
  return is_opened(QString::fromStdString(path.string()));
}

void OpenedFilesStore::mark_opened(const QString& path)
{
  const QString n = normalize(path);
  if (n.isEmpty()) {
    return;
  }
  const auto [it, inserted] = opened_.insert(n.toStdString());
  (void)it;
  if (!inserted) {
    return;
  }
  if (static_cast<int>(opened_.size()) > kMaxEntries) {
    // Drop arbitrary older entries if unbounded growth (path set has no order).
    // Prefer keeping the newest mark: erase one other key.
    for (auto e = opened_.begin(); e != opened_.end(); ++e) {
      if (*e != n.toStdString()) {
        opened_.erase(e);
        break;
      }
    }
  }
  save();
  emit membership_changed(QStringList{n});
}

void OpenedFilesStore::mark_opened(const std::filesystem::path& path)
{
  mark_opened(QString::fromStdString(path.string()));
}

void OpenedFilesStore::mark_opened(const std::vector<std::filesystem::path>& paths)
{
  QStringList changed;
  changed.reserve(static_cast<int>(paths.size()));
  for (const auto& p : paths) {
    const QString n = normalize(p);
    if (n.isEmpty()) {
      continue;
    }
    if (opened_.insert(n.toStdString()).second) {
      changed << n;
    }
  }
  if (changed.isEmpty()) {
    return;
  }
  while (static_cast<int>(opened_.size()) > kMaxEntries && !opened_.empty()) {
    opened_.erase(opened_.begin());
  }
  save();
  emit membership_changed(changed);
}

void OpenedFilesStore::mark_unopened(const QString& path)
{
  const QString n = normalize(path);
  if (n.isEmpty()) {
    return;
  }
  if (opened_.erase(n.toStdString()) == 0) {
    return;
  }
  save();
  emit membership_changed(QStringList{n});
}

void OpenedFilesStore::mark_unopened(const std::filesystem::path& path)
{
  mark_unopened(QString::fromStdString(path.string()));
}

void OpenedFilesStore::mark_unopened(const std::vector<std::filesystem::path>& paths)
{
  QStringList changed;
  for (const auto& p : paths) {
    const QString n = normalize(p);
    if (n.isEmpty()) {
      continue;
    }
    if (opened_.erase(n.toStdString()) > 0) {
      changed << n;
    }
  }
  if (changed.isEmpty()) {
    return;
  }
  save();
  emit membership_changed(changed);
}

void OpenedFilesStore::clear()
{
  if (opened_.empty()) {
    return;
  }
  opened_.clear();
  save();
  emit store_cleared();
}

void OpenedFilesStore::seed_from_open_history_if_empty()
{
  if (!opened_.empty()) {
    return;
  }
  const auto entries = open_history().entries();
  if (entries.empty()) {
    return;
  }
  QStringList changed;
  for (const auto& e : entries) {
    for (const QString& p : e.paths) {
      const QString n = normalize(p);
      if (n.isEmpty()) {
        continue;
      }
      if (opened_.insert(n.toStdString()).second) {
        changed << n;
      }
    }
  }
  if (changed.isEmpty()) {
    return;
  }
  save();
  emit membership_changed(changed);
}

OpenedFilesStore& opened_files_store()
{
  static OpenedFilesStore instance(OpenedFilesStore::default_path());
  return instance;
}

} // namespace dirtoo::app

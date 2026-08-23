// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <filesystem>
#include <unordered_set>
#include <vector>

namespace dirtoo::app {

/// Persistent “opened / seen” marks for files — independent of OpenHistory
/// (recently-opened log). Used for read/unread-style cues in the file view.
///
/// Keys are absolute path strings. Renames are not tracked (v1).
class OpenedFilesStore : public QObject {
  Q_OBJECT
public:
  explicit OpenedFilesStore(std::filesystem::path file, QObject* parent = nullptr);

  [[nodiscard]] static std::filesystem::path default_path();

  [[nodiscard]] bool is_opened(const QString& path) const;
  [[nodiscard]] bool is_opened(const std::filesystem::path& path) const;

  void mark_opened(const QString& path);
  void mark_opened(const std::filesystem::path& path);
  void mark_opened(const std::vector<std::filesystem::path>& paths);

  void mark_unopened(const QString& path);
  void mark_unopened(const std::filesystem::path& path);
  void mark_unopened(const std::vector<std::filesystem::path>& paths);

  void clear();

  /// Import paths from the recent-open log once (only if this store is empty).
  void seed_from_open_history_if_empty();

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }
  [[nodiscard]] int count() const { return static_cast<int>(opened_.size()); }

signals:
  void membership_changed(const QStringList& paths);
  void store_cleared();

private:
  void load();
  void save() const;
  [[nodiscard]] static QString normalize(const QString& path);
  [[nodiscard]] static QString normalize(const std::filesystem::path& path);

  std::filesystem::path path_;
  std::unordered_set<std::string> opened_;
  bool loaded_ = false;
};

[[nodiscard]] OpenedFilesStore& opened_files_store();

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <QObject>
#include <QString>

#include <filesystem>
#include <optional>
#include <unordered_map>

class QProcess;

namespace dirtoo::archive {

enum class ExtractStatus {
  Idle,
  Working,
  Ready,
  Failed,
};

/// Extracts archives into a cache directory and maps archive Locations to
/// real filesystem paths that can be listed with std::filesystem.
///
/// Extraction prefers libarchive when built with DIRTOO_HAS_LIBARCHIVE; else
/// external tools (bsdtar, tar, unzip, 7z).
class ArchiveManager : public QObject {
  Q_OBJECT

public:
  explicit ArchiveManager(QObject* parent = nullptr);
  explicit ArchiveManager(std::filesystem::path cache_root, QObject* parent = nullptr);

  /// Ensure archive contents are available. Emits ready/failed asynchronously
  /// if extraction is required; may emit ready immediately on cache hit.
  void open(const fs::Location& archive_location);

  /// Local directory containing the fully extracted archive tree (archive root).
  [[nodiscard]] std::optional<std::filesystem::path>
  extracted_root(const fs::Location& archive_location) const;

  /// Directory to list for this location (extracted_root / entry_path).
  [[nodiscard]] std::optional<std::filesystem::path>
  resolved_directory(const fs::Location& location) const;

  [[nodiscard]] ExtractStatus status(const fs::Location& archive_location) const;
  [[nodiscard]] QString last_error(const fs::Location& archive_location) const;

signals:
  void extraction_started(const dirtoo::fs::Location& archive_location);
  void extraction_ready(const dirtoo::fs::Location& archive_location,
                        const std::filesystem::path& extracted_root);
  void extraction_failed(const dirtoo::fs::Location& archive_location, const QString& message);

private:
  struct Entry {
    ExtractStatus status = ExtractStatus::Idle;
    std::filesystem::path cache_dir;
    QString error;
    QProcess* process = nullptr;
  };

  [[nodiscard]] std::filesystem::path cache_dir_for(const std::filesystem::path& archive_file) const;
  void start_extract(const fs::Location& archive_location, Entry& entry);
  void finish_ok(const fs::Location& archive_location);
  void finish_fail(const fs::Location& archive_location, const QString& message);

  std::filesystem::path cache_root_;
  std::unordered_map<std::string, Entry> entries_; // key = archive file path
};

} // namespace dirtoo::archive

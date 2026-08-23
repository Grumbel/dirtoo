// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <filesystem>
#include <functional>
#include <vector>

namespace dirtoo::app {

enum class OperationKind {
  Copy,
  Move,
  Rename,
  Delete,
  Mkdir,
  Mkfile,
  Symlink,
  Swap,
  Permissions,
  Other,
};

[[nodiscard]] QString operation_kind_label(OperationKind kind);
[[nodiscard]] OperationKind operation_kind_from_string(const QString& s);
[[nodiscard]] QString operation_kind_to_string(OperationKind kind);

struct OperationItem {
  QString source;
  QString destination;
  bool skipped = false;
};

struct OperationHistoryEntry {
  qint64 id = 0;
  QDateTime when;
  OperationKind kind = OperationKind::Other;
  QStringList sources;
  QString destination;
  QStringList destinations;
  std::vector<OperationItem> items;
  QString outcome;
  QString detail;
  int completed = 0;
  int skipped = 0;
};

class OperationsHistory {
public:
  explicit OperationsHistory(std::filesystem::path db_path);
  ~OperationsHistory();

  OperationsHistory(const OperationsHistory&) = delete;
  OperationsHistory& operator=(const OperationsHistory&) = delete;

  [[nodiscard]] static std::filesystem::path default_path();

  void record(OperationHistoryEntry entry);

  void record_simple(OperationKind kind, const std::vector<std::filesystem::path>& sources,
                     const std::filesystem::path& destination, bool ok,
                     const QString& detail = {});

  [[nodiscard]] std::vector<OperationHistoryEntry> entries() const;
  void clear();

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
  void open_db();
  void close_db();

  std::filesystem::path path_;
  void* db_ = nullptr;
  static constexpr int kMaxEntries = 1000;
};

[[nodiscard]] OperationsHistory& operations_history();

void show_operations_history_dialog(
    QWidget* parent, std::function<void(const QString& directory)> on_go_to_folder = {});

} // namespace dirtoo::app

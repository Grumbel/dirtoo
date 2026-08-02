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
  Permissions, // reserved for future editable permissions
  Other,
};

[[nodiscard]] QString operation_kind_label(OperationKind kind);
[[nodiscard]] OperationKind operation_kind_from_string(const QString& s);
[[nodiscard]] QString operation_kind_to_string(OperationKind kind);

struct OperationHistoryEntry {
  QDateTime when;
  OperationKind kind = OperationKind::Other;
  QStringList sources;
  QString destination; // may be empty (e.g. delete)
  QString outcome;     // "success" | "skipped" | "failed" | "cancelled" | …
  QString detail;      // optional error or note
};

/// Append-only log of filesystem mutations performed by the GUI.
/// No rollback — browse and “go to folder” only.
class OperationsHistory {
public:
  explicit OperationsHistory(std::filesystem::path file);

  [[nodiscard]] static std::filesystem::path default_path();

  void record(OperationHistoryEntry entry);

  void record_simple(OperationKind kind, const std::vector<std::filesystem::path>& sources,
                     const std::filesystem::path& destination, bool ok,
                     const QString& detail = {});

  [[nodiscard]] std::vector<OperationHistoryEntry> entries() const;
  void clear();

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
  void load();
  void save() const;

  std::filesystem::path path_;
  std::vector<OperationHistoryEntry> entries_;
  static constexpr int kMaxEntries = 500;
};

[[nodiscard]] OperationsHistory& operations_history();

/// Filterable dialog; optional callback to navigate to a directory.
void show_operations_history_dialog(
    QWidget* parent, std::function<void(const QString& directory)> on_go_to_folder = {});

} // namespace dirtoo::app

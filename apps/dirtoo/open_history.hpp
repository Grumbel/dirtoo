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

class QMenu;

namespace dirtoo::app {

/// One “opened with application” event (possibly multiple files in one launch).
struct OpenHistoryEntry {
  QDateTime when;
  QString app_id;   // desktop id, or "default" / "command:…"
  QString app_name; // human label
  QString app_icon; // theme icon name (may be empty)
  QStringList paths;
};

/// Persistent recent-open log (files + application used).
class OpenHistory {
public:
  explicit OpenHistory(std::filesystem::path file);

  [[nodiscard]] static std::filesystem::path default_path();

  void record(OpenHistoryEntry entry);
  void record_open(const QString& app_id, const QString& app_name, const QString& app_icon,
                   const std::vector<std::filesystem::path>& paths);

  [[nodiscard]] std::vector<OpenHistoryEntry> entries() const;
  void clear();

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
  void load();
  void save() const;

  std::filesystem::path path_;
  std::vector<OpenHistoryEntry> entries_;
  static constexpr int kMaxEntries = 200;
};

/// Shared instance used by open_with helpers and the main window.
[[nodiscard]] OpenHistory& open_history();

/// Show searchable open-history dialog (re-open / go to folder).
/// If on_go_to_folder is set, it is called with a directory path instead of open_default.
void show_open_history_dialog(QWidget* parent,
                              std::function<void(const QString& directory)> on_go_to_folder = {});

/// Populate a submenu with recent open events (most recent first).
void populate_recent_opens_menu(QMenu* menu, int limit = 20);

} // namespace dirtoo::app

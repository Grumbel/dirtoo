// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/sets/file_set_store.hpp"

#include <QObject>
#include <QString>
#include <QWidget>

#include <optional>
#include <string>
#include <vector>

namespace dirtoo::app {

/// Orchestrates persistent ad-hoc file sets (create / toggle / extend).
class FileSetController : public QObject {
  Q_OBJECT
public:
  explicit FileSetController(QObject* parent = nullptr);

  void set_dialog_parent(QWidget* parent);

  /// Single-gesture set control (Ctrl+G):
  /// - none in a set → create new set with selection
  /// - some in a set → put all selection into that set (first/majority shared set)
  /// - all in the same set → remove selection from that set (delete set if empty)
  /// - all in sets but different sets → remove each from its set
  QString toggle_set_for_selection(const std::vector<dirtoo::fs::FileInfo>& selection);

  /// Always create a brand-new set from selection (context menu).
  QString create_set_from_selection(const std::vector<dirtoo::fs::FileInfo>& selection);

  /// Add selection to @p set_id (exclusive membership moves paths from other sets).
  bool add_selection_to_set(const std::vector<dirtoo::fs::FileInfo>& selection,
                            const QString& set_id);

  /// Remove selection from whichever set each file is in.
  bool remove_selection_from_set(const std::vector<dirtoo::fs::FileInfo>& selection);

  /// Add selection to the most recently created/used set (if any).
  bool add_selection_to_last_set(const std::vector<dirtoo::fs::FileInfo>& selection);

  [[nodiscard]] QString last_set_id() const { return last_set_id_; }
  void set_last_set_id(const QString& id) { last_set_id_ = id; }

  [[nodiscard]] std::optional<dirtoo::sets::FileSet>
  resolve_query(std::string_view query, std::string* error = nullptr);

  [[nodiscard]] bool ensure_store(std::string* error = nullptr);

  [[nodiscard]] dirtoo::sets::FileSetStore& store();

  [[nodiscard]] QStringList member_paths_for_path(const QString& path_key);

signals:
  void status_message(const QString& message, int timeout_ms = 4000);
  void set_created(const QString& set_id, int member_count);
  void set_updated(const QString& set_id, int member_count);
  void set_dissolved(const QString& set_id);

private:
  [[nodiscard]] static std::string path_key_for(const dirtoo::fs::FileInfo& fi);

  QWidget* dialog_parent_ = nullptr;
  dirtoo::sets::FileSetStore store_;
  QString last_set_id_;
};

} // namespace dirtoo::app

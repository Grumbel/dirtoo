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

/// Orchestrates persistent ad-hoc file sets (create from selection, last set, open).
class FileSetController : public QObject {
  Q_OBJECT
public:
  explicit FileSetController(QObject* parent = nullptr);

  void set_dialog_parent(QWidget* parent);

  /// Create a new anonymous set from @p selection; stores last_set_id_.
  /// Returns the set id (empty on failure).
  QString create_set_from_selection(const std::vector<dirtoo::fs::FileInfo>& selection);

  /// Add selection to the most recently created/used set (if any).
  /// Returns true if at least one path was added.
  bool add_selection_to_last_set(const std::vector<dirtoo::fs::FileInfo>& selection);

  [[nodiscard]] QString last_set_id() const { return last_set_id_; }
  void set_last_set_id(const QString& id) { last_set_id_ = id; }

  /// Resolve set:// query (id or unique label) to a set id.
  [[nodiscard]] std::optional<dirtoo::sets::FileSet>
  resolve_query(std::string_view query, std::string* error = nullptr);

  /// Ensure the store is open (shared process DB).
  [[nodiscard]] bool ensure_store(std::string* error = nullptr);

  [[nodiscard]] dirtoo::sets::FileSetStore& store();

signals:
  void status_message(const QString& message, int timeout_ms = 4000);
  void set_created(const QString& set_id, int member_count);
  void set_updated(const QString& set_id, int member_count);

private:
  [[nodiscard]] static std::string path_key_for(const dirtoo::fs::FileInfo& fi);

  QWidget* dialog_parent_ = nullptr;
  dirtoo::sets::FileSetStore store_;
  QString last_set_id_;
};

} // namespace dirtoo::app

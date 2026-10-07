// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirops/trash.hpp"

#include <QDialog>

#include <vector>

class QPushButton;
class QTableWidget;
class QLabel;

namespace dirtoo::app {

/// Browse the freedesktop.org trash (home trash and the volume trashes of the
/// user): restore, delete permanently, or empty. All filesystem work runs on
/// the I/O pool; the dialog never blocks on a slow volume.
class TrashDialog : public QDialog {
  Q_OBJECT

public:
  explicit TrashDialog(QWidget* parent = nullptr);

signals:
  /// Something was restored (or removed): the main view should reload.
  void filesystem_changed();

private:
  void reload();
  void restore_selected();
  void delete_selected();
  void empty_trash();
  void update_buttons();
  [[nodiscard]] std::vector<dirops::TrashEntry> selected_entries() const;
  /// Runs @p work (restore/delete) for @p entries on the I/O pool, records the
  /// operations history, reports failures, then reloads.
  void run_on_entries(const std::vector<dirops::TrashEntry>& entries, bool restore);

  QTableWidget* table_ = nullptr;
  QLabel* status_ = nullptr;
  QPushButton* restore_btn_ = nullptr;
  QPushButton* delete_btn_ = nullptr;
  QPushButton* empty_btn_ = nullptr;
  std::vector<dirops::TrashEntry> entries_;
  bool busy_ = false;
};

} // namespace dirtoo::app

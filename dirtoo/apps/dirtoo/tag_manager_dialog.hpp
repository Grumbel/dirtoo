// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>

class QLabel;
class QTreeWidget;
class QPushButton;

namespace dirtoo::app {

/// List all tag definitions and rename them (TagStore rename keeps file links).
class TagManagerDialog : public QDialog {
  Q_OBJECT
public:
  explicit TagManagerDialog(QWidget* parent = nullptr);

signals:
  /// Emitted after a successful rename so views can clear chip caches.
  void tags_changed();

private slots:
  void reload();
  void rename_selected();
  void delete_selected();
  void on_selection_changed();

private:
  QTreeWidget* tree_ = nullptr;
  QLabel* status_ = nullptr;
  QPushButton* rename_btn_ = nullptr;
  QPushButton* delete_btn_ = nullptr;
};

/// Show the Tag Manager dialog (modeless, deletes on close).
void show_tag_manager_dialog(QWidget* parent);

} // namespace dirtoo::app

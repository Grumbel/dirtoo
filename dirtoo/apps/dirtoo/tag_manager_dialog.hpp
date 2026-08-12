// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>

class QLabel;
class QTreeWidget;
class QPushButton;

namespace dirtoo::app {

/// List tag definitions; rename, edit label/color/badge, delete.
/// Tag *name* is the stable normalized key (filter language, DB).
/// *Label* is optional display text; *color*/*badge* decorate chips in views.
class TagManagerDialog : public QDialog {
  Q_OBJECT
public:
  explicit TagManagerDialog(QWidget* parent = nullptr);

signals:
  /// Emitted after rename / meta / delete so views can clear chip caches.
  void tags_changed();

private slots:
  void reload();
  void rename_selected();
  void edit_selected();
  void delete_selected();
  void on_selection_changed();

private:
  QTreeWidget* tree_ = nullptr;
  QLabel* status_ = nullptr;
  QPushButton* rename_btn_ = nullptr;
  QPushButton* edit_btn_ = nullptr;
  QPushButton* delete_btn_ = nullptr;
};

void show_tag_manager_dialog(QWidget* parent);

} // namespace dirtoo::app

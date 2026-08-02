// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

class QListWidget;
class QListWidgetItem;
class QWidget;

namespace dirtoo::app {

class UDisksClient;

/// Owns UDisksClient and populates a devices QListWidget. Emits navigation /
/// status so MainWindow does not embed volume list logic.
class DevicesController : public QObject {
  Q_OBJECT

public:
  explicit DevicesController(QObject* parent = nullptr);

  void set_list_widget(QListWidget* list);
  void set_parent_widget(QWidget* parent_for_menus);

  void refresh();
  [[nodiscard]] UDisksClient* client() const noexcept { return client_; }

public slots:
  void on_item_activated(QListWidgetItem* item);
  void on_context_menu(const QPoint& pos);

signals:
  void open_path(const QString& path);
  void status_message(const QString& text);

private slots:
  void on_volumes_changed();
  void on_operation_finished(const QString& object_path, bool ok, const QString& message);

private:
  void rebuild_list();

  UDisksClient* client_ = nullptr;
  QListWidget* list_ = nullptr;
  QWidget* menu_parent_ = nullptr;
};

} // namespace dirtoo::app

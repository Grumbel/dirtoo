// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "devices_controller.hpp"

#include "size_format.hpp"
#include "udisks_client.hpp"

#include <QIcon>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QPalette>
#include <QPoint>

namespace dirtoo::app {
namespace {

QIcon theme_icon(const char* name, const char* fallback = "drive-harddisk")
{
  return QIcon::fromTheme(QString::fromUtf8(name), QIcon::fromTheme(QString::fromUtf8(fallback)));
}

} // namespace

DevicesController::DevicesController(QObject* parent)
    : QObject(parent)
    , client_(new UDisksClient(this))
{
  connect(client_, &UDisksClient::volumes_changed, this, &DevicesController::on_volumes_changed);
  connect(client_, &UDisksClient::operation_finished, this,
          &DevicesController::on_operation_finished);
}

void DevicesController::set_list_widget(QListWidget* list)
{
  list_ = list;
}

void DevicesController::set_parent_widget(QWidget* parent_for_menus)
{
  menu_parent_ = parent_for_menus;
}

void DevicesController::refresh()
{
  if (client_ != nullptr) {
    client_->refresh();
  }
}

void DevicesController::on_volumes_changed()
{
  rebuild_list();
}

void DevicesController::rebuild_list()
{
  if (list_ == nullptr || client_ == nullptr) {
    return;
  }
  list_->clear();
  list_->setVisible(true);
  const bool avail = client_->available();
  const auto vols = client_->volumes();
  if (!avail) {
    auto* item = new QListWidgetItem(QStringLiteral("Disks unavailable"));
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    list_->addItem(item);
    return;
  }
  if (vols.isEmpty()) {
    auto* item = new QListWidgetItem(QStringLiteral("No volumes"));
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    list_->addItem(item);
    return;
  }
  for (const auto& v : vols) {
    QString text = v.label;
    if (v.mounted && !v.mount_point.isEmpty()) {
      text += QStringLiteral("  —  ") + v.mount_point;
    } else if (!v.mounted) {
      text += QStringLiteral("  (not mounted)");
    }
    auto* item = new QListWidgetItem(text);
    item->setData(Qt::UserRole, v.mount_point);
    item->setData(Qt::UserRole + 1, v.object_path);
    item->setData(Qt::UserRole + 2, v.mounted);
    item->setData(Qt::UserRole + 3, v.ejectable || v.removable || v.optical);
    QString tip = v.device;
    if (!v.fstype.isEmpty()) {
      tip += QStringLiteral(" (") + v.fstype + QLatin1Char(')');
    }
    if (v.size > 0) {
      tip += QStringLiteral(" · ") + format_byte_size(v.size);
    }
    item->setToolTip(tip);
    QString icon_name = QStringLiteral("drive-harddisk");
    if (v.optical) {
      icon_name = QStringLiteral("drive-optical");
    } else if (v.removable || v.ejectable) {
      icon_name = QStringLiteral("drive-removable-media");
    }
    item->setIcon(QIcon::fromTheme(icon_name, QIcon::fromTheme(QStringLiteral("folder"))));
    if (!v.mounted && menu_parent_ != nullptr) {
      item->setForeground(menu_parent_->palette().color(QPalette::Disabled, QPalette::WindowText));
    }
    list_->addItem(item);
  }
}

void DevicesController::on_item_activated(QListWidgetItem* item)
{
  if (item == nullptr || client_ == nullptr) {
    return;
  }
  const QString mp = item->data(Qt::UserRole).toString();
  const bool mounted = item->data(Qt::UserRole + 2).toBool();
  const QString object_path = item->data(Qt::UserRole + 1).toString();
  if (mounted && !mp.isEmpty()) {
    emit open_path(mp);
    return;
  }
  if (!mounted && !object_path.isEmpty()) {
    emit status_message(QStringLiteral("Mounting…"));
    client_->mount(object_path);
  }
}

void DevicesController::on_context_menu(const QPoint& pos)
{
  if (list_ == nullptr || client_ == nullptr) {
    return;
  }
  QListWidgetItem* item = list_->itemAt(pos);
  if (item == nullptr) {
    return;
  }
  const QString object_path = item->data(Qt::UserRole + 1).toString();
  if (object_path.isEmpty()) {
    return;
  }
  const bool mounted = item->data(Qt::UserRole + 2).toBool();
  const bool can_eject = item->data(Qt::UserRole + 3).toBool();

  QMenu menu(menu_parent_ != nullptr ? menu_parent_ : list_);
  if (mounted) {
    menu.addAction(theme_icon("media-eject", "media-eject"), QStringLiteral("Unmount"), this,
                   [this, object_path] {
                     emit status_message(QStringLiteral("Unmounting…"));
                     client_->unmount(object_path);
                   });
  } else {
    menu.addAction(theme_icon("media-mount", "drive-harddisk"), QStringLiteral("Mount"), this,
                   [this, object_path] {
                     emit status_message(QStringLiteral("Mounting…"));
                     client_->mount(object_path);
                   });
  }
  if (can_eject) {
    menu.addAction(theme_icon("media-eject"), QStringLiteral("Eject"), this, [this, object_path] {
      emit status_message(QStringLiteral("Ejecting…"));
      client_->eject(object_path);
    });
  }
  if (menu.actions().isEmpty()) {
    return;
  }
  menu.exec(list_->mapToGlobal(pos));
}

void DevicesController::on_operation_finished(const QString& object_path, bool ok,
                                              const QString& message)
{
  (void)object_path;
  if (!ok) {
    emit status_message(QStringLiteral("Disk operation failed: %1").arg(message));
    return;
  }
  if (!message.isEmpty() && message.startsWith(QLatin1Char('/'))) {
    emit status_message(QStringLiteral("Mounted at %1").arg(message));
    emit open_path(message);
    return;
  }
  if (message.isEmpty()) {
    emit status_message(QStringLiteral("Disk operation finished"));
  } else {
    emit status_message(message);
  }
}

} // namespace dirtoo::app

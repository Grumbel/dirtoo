// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "udisks_client.hpp"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDebug>
#include <QVariant>

namespace dirtoo::app {
namespace {

constexpr auto kService = "org.freedesktop.UDisks2";

QString decode_ay(const QByteArray& bytes)
{
  int end = bytes.indexOf('\0');
  if (end < 0) {
    end = bytes.size();
  }
  if (end <= 0) {
    return {};
  }
  return QString::fromUtf8(bytes.constData(), end);
}

QVariant props_get(const QString& object_path, const QString& iface, const QString& prop)
{
  QDBusInterface props(kService, object_path, QStringLiteral("org.freedesktop.DBus.Properties"),
                       QDBusConnection::systemBus());
  if (!props.isValid()) {
    return {};
  }
  QDBusReply<QVariant> reply = props.call(QStringLiteral("Get"), iface, prop);
  if (!reply.isValid()) {
    return {};
  }
  return reply.value();
}

QString first_mount_point(const QVariant& mount_points_var)
{
  // Prefer QDBusArgument aay
  if (mount_points_var.canConvert<QDBusArgument>()) {
    const QDBusArgument arg = mount_points_var.value<QDBusArgument>();
    if (arg.currentType() == QDBusArgument::ArrayType) {
      arg.beginArray();
      while (!arg.atEnd()) {
        QByteArray bytes;
        arg >> bytes;
        const QString mp = decode_ay(bytes);
        if (!mp.isEmpty()) {
          arg.endArray();
          return mp;
        }
      }
      arg.endArray();
    }
  }

  const QVariantList list = mount_points_var.toList();
  for (const QVariant& item : list) {
    if (item.canConvert<QByteArray>()) {
      const QString mp = decode_ay(item.toByteArray());
      if (!mp.isEmpty()) {
        return mp;
      }
    }
  }
  return {};
}

VolumeInfo read_block(const QString& object_path)
{
  VolumeInfo v;
  v.object_path = object_path;

  constexpr auto kBlock = "org.freedesktop.UDisks2.Block";
  constexpr auto kFs = "org.freedesktop.UDisks2.Filesystem";

  const QVariant device = props_get(object_path, kBlock, QStringLiteral("Device"));
  if (device.canConvert<QByteArray>()) {
    v.device = decode_ay(device.toByteArray());
  }

  v.label = props_get(object_path, kBlock, QStringLiteral("IdLabel")).toString();
  v.uuid = props_get(object_path, kBlock, QStringLiteral("IdUUID")).toString();
  v.fstype = props_get(object_path, kBlock, QStringLiteral("IdType")).toString();
  v.size = props_get(object_path, kBlock, QStringLiteral("Size")).toULongLong();
  if (v.label.isEmpty()) {
    v.label = props_get(object_path, kBlock, QStringLiteral("HintName")).toString();
  }

  const QVariant mps = props_get(object_path, kFs, QStringLiteral("MountPoints"));
  if (mps.isValid()) {
    v.mount_point = first_mount_point(mps);
    v.mounted = !v.mount_point.isEmpty();
  }

  if (v.label.isEmpty()) {
    v.label = v.device;
  }
  if (v.label.isEmpty()) {
    v.label = QStringLiteral("Volume");
  }
  return v;
}

} // namespace

UDisksClient::UDisksClient(QObject* parent)
    : QObject(parent)
{
}

void UDisksClient::refresh()
{
  volumes_.clear();
  available_ = false;

  QDBusInterface manager(kService, QStringLiteral("/org/freedesktop/UDisks2/Manager"),
                         QStringLiteral("org.freedesktop.UDisks2.Manager"),
                         QDBusConnection::systemBus());
  if (!manager.isValid()) {
    qInfo().noquote() << QStringLiteral("udisks: Manager unavailable: %1")
                             .arg(manager.lastError().message());
    emit volumes_changed();
    return;
  }

  // GetBlockDevices(a{sv} options) → ao
  QDBusMessage call = manager.call(QStringLiteral("GetBlockDevices"), QVariantMap{});
  if (call.type() == QDBusMessage::ErrorMessage) {
    qInfo().noquote() << QStringLiteral("udisks: GetBlockDevices failed: %1")
                             .arg(call.errorMessage());
    emit volumes_changed();
    return;
  }
  available_ = true;

  if (call.arguments().isEmpty()) {
    emit volumes_changed();
    return;
  }

  QList<QDBusObjectPath> paths;
  const QVariant arg0 = call.arguments().at(0);
  if (arg0.canConvert<QDBusArgument>()) {
    const QDBusArgument a = arg0.value<QDBusArgument>();
    a.beginArray();
    while (!a.atEnd()) {
      QDBusObjectPath p;
      a >> p;
      paths.append(p);
    }
    a.endArray();
  } else {
    const QVariantList list = arg0.toList();
    for (const QVariant& item : list) {
      if (item.canConvert<QDBusObjectPath>()) {
        paths.append(item.value<QDBusObjectPath>());
      } else {
        paths.append(QDBusObjectPath(item.toString()));
      }
    }
  }

  for (const QDBusObjectPath& op : paths) {
    VolumeInfo v = read_block(op.path());
    // Phase 2a: only show mounted filesystems in the Devices list.
    if (v.mounted) {
      volumes_.push_back(std::move(v));
    }
  }

  emit volumes_changed();
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "udisks_client.hpp"

#include <algorithm>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDebug>
#include <QTimer>
#include <QVariant>

namespace dirtoo::app {
namespace {

constexpr auto kService = "org.freedesktop.UDisks2";
constexpr auto kObjectManagerPath = "/org/freedesktop/UDisks2";
constexpr auto kBlockIface = "org.freedesktop.UDisks2.Block";
constexpr auto kFsIface = "org.freedesktop.UDisks2.Filesystem";
constexpr auto kDriveIface = "org.freedesktop.UDisks2.Drive";

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

QString object_path_from_variant(const QVariant& v)
{
  if (v.canConvert<QDBusObjectPath>()) {
    return v.value<QDBusObjectPath>().path();
  }
  const QString s = v.toString();
  if (s.startsWith(QLatin1Char('/'))) {
    return s;
  }
  return {};
}

VolumeInfo read_block(const QString& object_path)
{
  VolumeInfo v;
  v.object_path = object_path;

  const QVariant device = props_get(object_path, kBlockIface, QStringLiteral("Device"));
  if (device.canConvert<QByteArray>()) {
    v.device = decode_ay(device.toByteArray());
  }

  v.label = props_get(object_path, kBlockIface, QStringLiteral("IdLabel")).toString();
  v.uuid = props_get(object_path, kBlockIface, QStringLiteral("IdUUID")).toString();
  v.fstype = props_get(object_path, kBlockIface, QStringLiteral("IdType")).toString();
  v.size = props_get(object_path, kBlockIface, QStringLiteral("Size")).toULongLong();
  if (v.label.isEmpty()) {
    v.label = props_get(object_path, kBlockIface, QStringLiteral("HintName")).toString();
  }

  const QString id_usage = props_get(object_path, kBlockIface, QStringLiteral("IdUsage")).toString();
  const bool hint_ignore = props_get(object_path, kBlockIface, QStringLiteral("HintIgnore")).toBool();

  const QVariant mps = props_get(object_path, kFsIface, QStringLiteral("MountPoints"));
  v.has_filesystem = mps.isValid() || id_usage == QLatin1String("filesystem");
  if (mps.isValid()) {
    v.mount_point = first_mount_point(mps);
    v.mounted = !v.mount_point.isEmpty();
  }

  v.drive_path = object_path_from_variant(props_get(object_path, kBlockIface, QStringLiteral("Drive")));
  if (!v.drive_path.isEmpty() && v.drive_path != QLatin1String("/")) {
    v.removable = props_get(v.drive_path, kDriveIface, QStringLiteral("Removable")).toBool();
    v.ejectable = props_get(v.drive_path, kDriveIface, QStringLiteral("Ejectable")).toBool();
    const QString media = props_get(v.drive_path, kDriveIface, QStringLiteral("Media")).toString();
    const bool optical_flag =
        props_get(v.drive_path, kDriveIface, QStringLiteral("Optical")).toBool();
    v.optical = optical_flag || media.contains(QLatin1String("optical"), Qt::CaseInsensitive);
  }

  // Drop ignored / non-filesystem blocks (partition tables, swap without FS iface, etc.).
  if (hint_ignore || !v.has_filesystem) {
    v.has_filesystem = false;
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
  refresh_debounce_ = new QTimer(this);
  refresh_debounce_->setSingleShot(true);
  refresh_debounce_->setInterval(250);
  connect(refresh_debounce_, &QTimer::timeout, this, &UDisksClient::refresh);
  connect_object_manager();
}

UDisksClient::~UDisksClient() = default;

void UDisksClient::connect_object_manager()
{
  if (signals_connected_) {
    return;
  }
  auto& bus = QDBusConnection::systemBus();
  // ObjectManager lives on /org/freedesktop/UDisks2
  const bool added = bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kObjectManagerPath),
                                 QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                                 QStringLiteral("InterfacesAdded"), this, SLOT(schedule_refresh()));
  const bool removed =
      bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kObjectManagerPath),
                  QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                  QStringLiteral("InterfacesRemoved"), this, SLOT(schedule_refresh()));
  // Property changes on any UDisks object (mount point updates).
  const bool props = bus.connect(QString::fromLatin1(kService), QString(),
                                 QStringLiteral("org.freedesktop.DBus.Properties"),
                                 QStringLiteral("PropertiesChanged"), this, SLOT(schedule_refresh()));
  signals_connected_ = added || removed || props;
  if (!signals_connected_) {
    qInfo().noquote() << QStringLiteral("udisks: could not subscribe to ObjectManager signals");
  }
}

void UDisksClient::schedule_refresh()
{
  if (refresh_debounce_ != nullptr) {
    refresh_debounce_->start();
  }
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

  QDBusMessage call = manager.call(QStringLiteral("GetBlockDevices"), QVariantMap{});
  if (call.type() == QDBusMessage::ErrorMessage) {
    qInfo().noquote() << QStringLiteral("udisks: GetBlockDevices failed: %1")
                             .arg(call.errorMessage());
    emit volumes_changed();
    return;
  }
  available_ = true;
  connect_object_manager();

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
    if (!v.has_filesystem) {
      continue;
    }
    // Prefer showing mounted volumes and removable/unmounted media; skip quiet
    // system-only unmounted partitions (HintSystem + not removable + not mounted).
    const bool hint_system =
        props_get(v.object_path, kBlockIface, QStringLiteral("HintSystem")).toBool();
    if (!v.mounted && hint_system && !v.removable && !v.ejectable) {
      continue;
    }
    volumes_.push_back(std::move(v));
  }

  // Mounted first, then label.
  std::sort(volumes_.begin(), volumes_.end(), [](const VolumeInfo& a, const VolumeInfo& b) {
    if (a.mounted != b.mounted) {
      return a.mounted && !b.mounted;
    }
    return a.label.localeAwareCompare(b.label) < 0;
  });

  emit volumes_changed();
}

VolumeInfo UDisksClient::volume_for_path(const QString& object_path) const
{
  for (const auto& v : volumes_) {
    if (v.object_path == object_path) {
      return v;
    }
  }
  return {};
}

void UDisksClient::mount(const QString& object_path)
{
  if (object_path.isEmpty()) {
    emit operation_finished(object_path, false, QStringLiteral("empty object path"));
    return;
  }
  QDBusInterface fs(kService, object_path, QString::fromLatin1(kFsIface),
                    QDBusConnection::systemBus());
  if (!fs.isValid()) {
    emit operation_finished(object_path, false, fs.lastError().message());
    return;
  }
  // Mount(a{sv} options) → s mount_path
  QDBusPendingCall pending = fs.asyncCall(QStringLiteral("Mount"), QVariantMap{});
  auto* watcher = new QDBusPendingCallWatcher(pending, this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, object_path](QDBusPendingCallWatcher* w) {
            QDBusPendingReply<QString> reply = *w;
            w->deleteLater();
            if (reply.isError()) {
              emit operation_finished(object_path, false, reply.error().message());
            } else {
              emit operation_finished(object_path, true, reply.value());
            }
            schedule_refresh();
          });
}

void UDisksClient::unmount(const QString& object_path)
{
  if (object_path.isEmpty()) {
    emit operation_finished(object_path, false, QStringLiteral("empty object path"));
    return;
  }
  QDBusInterface fs(kService, object_path, QString::fromLatin1(kFsIface),
                    QDBusConnection::systemBus());
  if (!fs.isValid()) {
    emit operation_finished(object_path, false, fs.lastError().message());
    return;
  }
  QDBusPendingCall pending = fs.asyncCall(QStringLiteral("Unmount"), QVariantMap{});
  auto* watcher = new QDBusPendingCallWatcher(pending, this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, object_path](QDBusPendingCallWatcher* w) {
            QDBusPendingReply<> reply = *w;
            w->deleteLater();
            if (reply.isError()) {
              emit operation_finished(object_path, false, reply.error().message());
            } else {
              emit operation_finished(object_path, true, {});
            }
            schedule_refresh();
          });
}

void UDisksClient::eject(const QString& object_path)
{
  VolumeInfo v = volume_for_path(object_path);
  if (v.drive_path.isEmpty()) {
    // Re-read drive path in case volume list is stale.
    v.drive_path =
        object_path_from_variant(props_get(object_path, kBlockIface, QStringLiteral("Drive")));
  }
  if (v.drive_path.isEmpty() || v.drive_path == QLatin1String("/")) {
    emit operation_finished(object_path, false, QStringLiteral("no ejectable drive"));
    return;
  }
  QDBusInterface drive(kService, v.drive_path, QString::fromLatin1(kDriveIface),
                       QDBusConnection::systemBus());
  if (!drive.isValid()) {
    emit operation_finished(object_path, false, drive.lastError().message());
    return;
  }
  QDBusPendingCall pending = drive.asyncCall(QStringLiteral("Eject"), QVariantMap{});
  auto* watcher = new QDBusPendingCallWatcher(pending, this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, object_path](QDBusPendingCallWatcher* w) {
            QDBusPendingReply<> reply = *w;
            w->deleteLater();
            if (reply.isError()) {
              emit operation_finished(object_path, false, reply.error().message());
            } else {
              emit operation_finished(object_path, true, {});
            }
            schedule_refresh();
          });
}

} // namespace dirtoo::app

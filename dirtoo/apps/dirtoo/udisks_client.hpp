// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace dirtoo::app {

struct VolumeInfo {
  QString object_path;   ///< UDisks2 block device path
  QString drive_path;    ///< UDisks2 drive path (may be empty)
  QString device;        ///< e.g. /dev/sdb1
  QString label;
  QString uuid;
  QString fstype;
  QString mount_point;
  quint64 size = 0;
  bool mounted = false;
  bool has_filesystem = false;
  bool removable = false;
  bool ejectable = false;
  bool optical = false;
};

/// UDisks2 client for the Devices sidebar.
/// Lists filesystems (mounted and unmounted), supports Mount / Unmount / Eject
/// via async D-Bus calls, and refreshes on ObjectManager signals.
/// Degrades cleanly if the service is unavailable.
class UDisksClient : public QObject {
  Q_OBJECT

public:
  explicit UDisksClient(QObject* parent = nullptr);
  ~UDisksClient() override;

  void refresh();

  /// Async Filesystem.Mount. Emits operation_finished; on success refreshes volumes.
  void mount(const QString& object_path);
  /// Async Filesystem.Unmount.
  void unmount(const QString& object_path);
  /// Async Drive.Eject for the drive owning this block device.
  void eject(const QString& object_path);

  [[nodiscard]] QVector<VolumeInfo> volumes() const { return volumes_; }
  [[nodiscard]] bool available() const { return available_; }

  [[nodiscard]] VolumeInfo volume_for_path(const QString& object_path) const;

signals:
  void volumes_changed();
  /// Result of mount/unmount/eject. message is empty on success or a human error.
  void operation_finished(const QString& object_path, bool ok, const QString& message);

private slots:
  void schedule_refresh();

private:
  void connect_object_manager();

  QVector<VolumeInfo> volumes_;
  bool available_ = false;
  bool signals_connected_ = false;
  class QTimer* refresh_debounce_ = nullptr;
};

} // namespace dirtoo::app

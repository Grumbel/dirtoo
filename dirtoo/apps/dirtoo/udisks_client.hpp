// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace dirtoo::app {

struct VolumeInfo {
  QString object_path;
  QString device;
  QString label;
  QString uuid;
  QString fstype;
  QString mount_point;
  quint64 size = 0;
  bool mounted = false;
};

/// Read-only UDisks2 client. Degrades cleanly if the service is unavailable.
class UDisksClient : public QObject {
  Q_OBJECT

public:
  explicit UDisksClient(QObject* parent = nullptr);

  void refresh();

  [[nodiscard]] QVector<VolumeInfo> volumes() const { return volumes_; }
  [[nodiscard]] bool available() const { return available_; }

signals:
  void volumes_changed();

private:
  QVector<VolumeInfo> volumes_;
  bool available_ = false;
};

} // namespace dirtoo::app

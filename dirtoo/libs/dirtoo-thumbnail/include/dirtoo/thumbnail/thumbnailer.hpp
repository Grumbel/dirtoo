// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <QObject>
#include <QString>

namespace dirtoo::thumbnail {

/// Placeholder for freedesktop Thumbnailer1 D-Bus client.
class Thumbnailer : public QObject {
  Q_OBJECT

public:
  explicit Thumbnailer(QObject* parent = nullptr);

  /// Returns the expected on-disk path for a large thumbnail, if any.
  [[nodiscard]] static QString cache_path_for(const fs::Location& location,
                                              const QString& flavor = QStringLiteral("large"));

  void request(const fs::Location& location, const QString& flavor = QStringLiteral("large"));

signals:
  void thumbnail_ready(const dirtoo::fs::Location& location, const QString& path);
  void thumbnail_failed(const dirtoo::fs::Location& location, const QString& message);
};

} // namespace dirtoo::thumbnail

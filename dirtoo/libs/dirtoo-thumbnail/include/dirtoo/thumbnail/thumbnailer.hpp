// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

#include <unordered_map>

class QDBusInterface;

namespace dirtoo::thumbnail {

/// Client for org.freedesktop.thumbnails.Thumbnailer1.
/// Falls back to reading an existing cache file when the service is unavailable.
class Thumbnailer : public QObject {
  Q_OBJECT

public:
  explicit Thumbnailer(QObject* parent = nullptr);
  ~Thumbnailer() override;

  /// Expected on-disk path for a thumbnail (XDG thumbnail spec).
  [[nodiscard]] static QString cache_path_for(const fs::Location& location,
                                              const QString& flavor = QStringLiteral("large"));

  /// Delete cached thumbnail PNGs for this location (normal/large/x-large/xx-large
  /// and fail/). Returns true if any file was removed.
  static bool remove_cache_for(const fs::Location& location);

  /// Queue thumbnail generation. Emits thumbnail_ready or thumbnail_failed.
  /// If @p force is true, remove existing cache files first so the generator
  /// rebuilds from the source (e.g. after changing the thumbnailer backend).
  void request(const fs::Location& location,
               const QString& mime_type = QStringLiteral("application/octet-stream"),
               const QString& flavor = QStringLiteral("large"), bool force = false);

  void cancel_all();

  /// Locations still waiting on Thumbnailer1 (Queued handles not Finished yet).
  [[nodiscard]] int in_flight_count() const;

  void request_many(const std::vector<fs::Location>& locations, const QStringList& mime_types,
                    const QString& flavor = QStringLiteral("large"), bool force = false);

signals:
  void thumbnail_ready(const dirtoo::fs::Location& location, const QString& path);
  void thumbnail_failed(const dirtoo::fs::Location& location, const QString& message);

private slots:
  void on_ready(uint handle, const QStringList& uris);
  void on_error(uint handle, const QStringList& uris, int error_code, const QString& message);
  void on_finished(uint handle);

private:
  void connect_signals();
  void emit_from_cache_or_fail(const fs::Location& location, const QString& flavor,
                               const QString& reason);

  QDBusInterface* iface_ = nullptr;
  bool service_available_ = false;

  // handle -> list of locations queued under that handle
  std::unordered_map<uint, std::vector<fs::Location>> pending_;
};

} // namespace dirtoo::thumbnail

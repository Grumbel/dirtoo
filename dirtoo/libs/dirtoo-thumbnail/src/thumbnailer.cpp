// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/thumbnail/thumbnailer.hpp"

#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

#include <utility>

namespace dirtoo::thumbnail {
namespace {

constexpr const char* kService = "org.freedesktop.thumbnails.Thumbnailer1";
constexpr const char* kPath = "/org/freedesktop/thumbnails/Thumbnailer1";
constexpr const char* kInterface = "org.freedesktop.thumbnails.Thumbnailer1";

} // namespace

Thumbnailer::Thumbnailer(QObject* parent)
    : QObject(parent)
{
  iface_ = new QDBusInterface(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                              QString::fromLatin1(kInterface), QDBusConnection::sessionBus(),
                              this);
  service_available_ = iface_->isValid();
  if (service_available_) {
    connect_signals();
  }
}

Thumbnailer::~Thumbnailer() = default;

void Thumbnailer::connect_signals()
{
  QDBusConnection::sessionBus().connect(
      QString::fromLatin1(kService), QString::fromLatin1(kPath), QString::fromLatin1(kInterface),
      QStringLiteral("Ready"), this, SLOT(on_ready(uint,QStringList)));
  QDBusConnection::sessionBus().connect(
      QString::fromLatin1(kService), QString::fromLatin1(kPath), QString::fromLatin1(kInterface),
      QStringLiteral("Error"), this, SLOT(on_error(uint,QStringList,int,QString)));
  QDBusConnection::sessionBus().connect(
      QString::fromLatin1(kService), QString::fromLatin1(kPath), QString::fromLatin1(kInterface),
      QStringLiteral("Finished"), this, SLOT(on_finished(uint)));
}

QString Thumbnailer::cache_path_for(const fs::Location& location, const QString& flavor)
{
  // Freedesktop thumbnail spec: MD5 of the canonical file URI.
  const QByteArray url = QByteArray::fromStdString(location.as_url());
  const QByteArray digest = QCryptographicHash::hash(url, QCryptographicHash::Md5).toHex();
  const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                       + QStringLiteral("/thumbnails/") + flavor;
  return base + QLatin1Char('/') + QString::fromLatin1(digest) + QStringLiteral(".png");
}

bool Thumbnailer::remove_cache_for(const fs::Location& location)
{
  bool removed = false;
  const QStringList flavors = {QStringLiteral("normal"), QStringLiteral("large"),
                               QStringLiteral("x-large"), QStringLiteral("xx-large"),
                               QStringLiteral("fail")};
  for (const QString& flavor : flavors) {
    const QString path = cache_path_for(location, flavor);
    if (QFileInfo::exists(path) && QFile::remove(path)) {
      removed = true;
    }
  }
  return removed;
}

void Thumbnailer::emit_from_cache_or_fail(const fs::Location& location, const QString& flavor,
                                          const QString& reason)
{
  const QString path = cache_path_for(location, flavor);
  if (QFileInfo::exists(path)) {
    emit thumbnail_ready(location, path);
  } else {
    emit thumbnail_failed(location, reason);
  }
}


void Thumbnailer::cancel_all()
{
  if (service_available_ && iface_ != nullptr && iface_->isValid()) {
    for (const auto& [handle, locs] : pending_) {
      (void)locs;
      iface_->call(QStringLiteral("Dequeue"), handle);
    }
  }
  pending_.clear();
}

int Thumbnailer::in_flight_count() const
{
  int n = 0;
  for (const auto& [handle, locs] : pending_) {
    (void)handle;
    n += static_cast<int>(locs.size());
  }
  return n;
}

void Thumbnailer::request(const fs::Location& location, const QString& mime_type,
                          const QString& flavor, bool force)
{
  if (force) {
    remove_cache_for(location);
  }
  // Fast path: already cached (skip when forcing a rebuild).
  const QString cached = cache_path_for(location, flavor);
  if (!force && QFileInfo::exists(cached)) {
    emit thumbnail_ready(location, cached);
    return;
  }

  if (!service_available_ || iface_ == nullptr || !iface_->isValid()) {
    emit_from_cache_or_fail(location, flavor,
                            QStringLiteral("thumbnail service unavailable"));
    return;
  }

  const QString uri = QString::fromStdString(location.as_url());
  const QDBusReply<uint> reply =
      iface_->call(QStringLiteral("Queue"), QStringList{uri}, QStringList{mime_type}, flavor,
                   QStringLiteral("default"), uint(0));
  if (!reply.isValid()) {
    emit_from_cache_or_fail(location, flavor, reply.error().message());
    return;
  }

  pending_[reply.value()].push_back(location);
}

void Thumbnailer::request_many(const std::vector<fs::Location>& locations,
                               const QStringList& mime_types, const QString& flavor, bool force)
{
  if (locations.empty()) {
    return;
  }

  QStringList uris;
  QStringList mimes;
  std::vector<fs::Location> need_queue;
  uris.reserve(static_cast<int>(locations.size()));
  mimes.reserve(static_cast<int>(locations.size()));

  for (std::size_t i = 0; i < locations.size(); ++i) {
    const auto& loc = locations[i];
    if (force) {
      remove_cache_for(loc);
    }
    const QString cached = cache_path_for(loc, flavor);
    if (!force && QFileInfo::exists(cached)) {
      emit thumbnail_ready(loc, cached);
      continue;
    }
    need_queue.push_back(loc);
    uris.push_back(QString::fromStdString(loc.as_url()));
    if (i < static_cast<std::size_t>(mime_types.size())) {
      mimes.push_back(mime_types[static_cast<int>(i)]);
    } else {
      mimes.push_back(QStringLiteral("application/octet-stream"));
    }
  }

  if (need_queue.empty()) {
    return;
  }

  if (!service_available_ || iface_ == nullptr || !iface_->isValid()) {
    for (const auto& loc : need_queue) {
      emit_from_cache_or_fail(loc, flavor, QStringLiteral("thumbnail service unavailable"));
    }
    return;
  }

  const QDBusReply<uint> reply =
      iface_->call(QStringLiteral("Queue"), uris, mimes, flavor, QStringLiteral("default"), uint(0));
  if (!reply.isValid()) {
    for (const auto& loc : need_queue) {
      emit_from_cache_or_fail(loc, flavor, reply.error().message());
    }
    return;
  }
  pending_[reply.value()] = std::move(need_queue);
}

void Thumbnailer::on_ready(uint handle, const QStringList& uris)
{
  const auto it = pending_.find(handle);
  for (const QString& uri : uris) {
    fs::Location loc;
    try {
      loc = fs::Location::from_url(uri.toStdString());
    } catch (...) {
      continue;
    }
    const QString path = cache_path_for(loc, QStringLiteral("large"));
    // Prefer matching flavor from cache existence of normal/large.
    const QString large = cache_path_for(loc, QStringLiteral("large"));
    const QString normal = cache_path_for(loc, QStringLiteral("normal"));
    if (QFileInfo::exists(large)) {
      emit thumbnail_ready(loc, large);
    } else if (QFileInfo::exists(normal)) {
      emit thumbnail_ready(loc, normal);
    } else if (QFileInfo::exists(path)) {
      emit thumbnail_ready(loc, path);
    } else {
      emit thumbnail_failed(loc, QStringLiteral("Ready signal but cache file missing"));
    }
  }
  (void)it;
}

void Thumbnailer::on_error(uint handle, const QStringList& uris, int error_code,
                           const QString& message)
{
  (void)handle;
  for (const QString& uri : uris) {
    try {
      const auto loc = fs::Location::from_url(uri.toStdString());
      emit thumbnail_failed(loc, QStringLiteral("[%1] %2").arg(error_code).arg(message));
    } catch (...) {
    }
  }
}

void Thumbnailer::on_finished(uint handle)
{
  pending_.erase(handle);
}

} // namespace dirtoo::thumbnail

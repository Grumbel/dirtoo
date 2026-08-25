// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/thumbnail/thumbnailer.hpp"

#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <cstring>
#include <optional>
#include <utility>

namespace dirtoo::thumbnail {
namespace {

constexpr const char* kService = "org.freedesktop.thumbnails.Thumbnailer1";
constexpr const char* kPath = "/org/freedesktop/thumbnails/Thumbnailer1";
constexpr const char* kInterface = "org.freedesktop.thumbnails.Thumbnailer1";

[[nodiscard]] quint32 read_be32(const char* p)
{
  const auto* u = reinterpret_cast<const unsigned char*>(p);
  return (quint32(u[0]) << 24) | (quint32(u[1]) << 16) | (quint32(u[2]) << 8) | quint32(u[3]);
}

[[nodiscard]] std::optional<QByteArray> png_text_chunk(const QString& path, const char* key)
{
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  const QByteArray data = f.read(2 * 1024 * 1024);
  static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  if (data.size() < 8 || std::memcmp(data.constData(), sig, 8) != 0) {
    return std::nullopt;
  }
  const QByteArray key_bytes(key);
  int off = 8;
  while (off + 12 <= data.size()) {
    const quint32 len = read_be32(data.constData() + off);
    if (off + 12 + static_cast<int>(len) > data.size()) {
      break;
    }
    const char* type = data.constData() + off + 4;
    const char* chunk = data.constData() + off + 8;
    if (std::memcmp(type, "tEXt", 4) == 0 && len > 0) {
      const int sep = static_cast<int>(QByteArray(chunk, static_cast<int>(len)).indexOf('\0'));
      if (sep > 0 && QByteArray(chunk, sep) == key_bytes) {
        return QByteArray(chunk + sep + 1, static_cast<int>(len) - sep - 1);
      }
    }
    if (std::memcmp(type, "IEND", 4) == 0) {
      break;
    }
    off += 12 + static_cast<int>(len);
  }
  return std::nullopt;
}

[[nodiscard]] bool cache_matches_source(const fs::Location& location, const QString& cache_path)
{
  if (!QFileInfo::exists(cache_path)) {
    return false;
  }
  if (!location.is_file() && !location.is_archive()) {
    return true;
  }
  const QString source_path = QString::fromStdString(location.as_path().string());
  if (source_path.isEmpty()) {
    return true;
  }
  const QFileInfo src(source_path);
  if (!src.exists()) {
    return true;
  }

  const qint64 src_mtime = src.lastModified().toSecsSinceEpoch();
  const qint64 src_size = src.size();

  if (const auto mt = png_text_chunk(cache_path, "Thumb::MTime")) {
    bool ok = false;
    const qint64 thumb_mtime = QString::fromUtf8(*mt).trimmed().toLongLong(&ok);
    if (ok && thumb_mtime != src_mtime) {
      return false;
    }
  } else {
    const QFileInfo cache(cache_path);
    if (src.lastModified() > cache.lastModified()) {
      return false;
    }
  }

  if (const auto sz = png_text_chunk(cache_path, "Thumb::Size")) {
    bool ok = false;
    const qint64 thumb_size = QString::fromUtf8(*sz).trimmed().toLongLong(&ok);
    if (ok && thumb_size != src_size) {
      return false;
    }
  }
  return true;
}

} // namespace

Thumbnailer::Thumbnailer(QObject* parent)
    : QObject(parent)
{
  (void)ensure_service();
}

Thumbnailer::~Thumbnailer() = default;

bool Thumbnailer::ensure_service()
{
  if (iface_ != nullptr && iface_->isValid()) {
    service_available_ = true;
    return true;
  }

  // Activate a provider if one is registered (e.g. tumblerd.service).
  if (auto* bus_iface = QDBusConnection::sessionBus().interface()) {
    bus_iface->startService(QString::fromLatin1(kService));
  }

  if (iface_ != nullptr) {
    delete iface_;
    iface_ = nullptr;
    signals_connected_ = false;
  }
  iface_ = new QDBusInterface(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                              QString::fromLatin1(kInterface), QDBusConnection::sessionBus(),
                              this);
  service_available_ = iface_->isValid();
  if (service_available_ && !signals_connected_) {
    connect_signals();
    signals_connected_ = true;
  }
  return service_available_;
}

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
  const QByteArray url = QByteArray::fromStdString(location.as_url());
  const QByteArray digest = QCryptographicHash::hash(url, QCryptographicHash::Md5).toHex();
  const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                       + QStringLiteral("/thumbnails/") + flavor;
  return base + QLatin1Char('/') + QString::fromLatin1(digest) + QStringLiteral(".png");
}

bool Thumbnailer::cache_is_fresh(const fs::Location& location, const QString& flavor)
{
  return cache_matches_source(location, cache_path_for(location, flavor));
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
  if (cache_matches_source(location, path)) {
    emit thumbnail_ready(location, path);
  } else {
    if (QFileInfo::exists(path)) {
      QFile::remove(path);
    }
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

void Thumbnailer::request(const fs::Location& location, const QString& mime_type,
                          const QString& flavor, bool force)
{
  if (force) {
    remove_cache_for(location);
  }
  const QString cached = cache_path_for(location, flavor);
  if (!force && cache_matches_source(location, cached)) {
    emit thumbnail_ready(location, cached);
    return;
  }
  if (!force && QFileInfo::exists(cached)) {
    QFile::remove(cached);
  }

  if (!ensure_service()) {
    emit_from_cache_or_fail(location, flavor,
                            QStringLiteral("thumbnail service unavailable"));
    return;
  }

  const QString uri = QString::fromStdString(location.as_url());
  const QDBusReply<uint> reply =
      iface_->call(QStringLiteral("Queue"), QStringList{uri}, QStringList{mime_type}, flavor,
                   QStringLiteral("default"), uint(0));
  if (!reply.isValid()) {
    service_available_ = false;
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
    if (!force && cache_matches_source(loc, cached)) {
      emit thumbnail_ready(loc, cached);
      continue;
    }
    if (!force && QFileInfo::exists(cached)) {
      QFile::remove(cached);
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

  if (!ensure_service()) {
    for (const auto& loc : need_queue) {
      emit_from_cache_or_fail(loc, flavor, QStringLiteral("thumbnail service unavailable"));
    }
    return;
  }

  const QDBusReply<uint> reply =
      iface_->call(QStringLiteral("Queue"), uris, mimes, flavor, QStringLiteral("default"), uint(0));
  if (!reply.isValid()) {
    service_available_ = false;
    for (const auto& loc : need_queue) {
      emit_from_cache_or_fail(loc, flavor, reply.error().message());
    }
    return;
  }
  pending_[reply.value()] = std::move(need_queue);
}

void Thumbnailer::on_ready(uint handle, const QStringList& uris)
{
  (void)handle;
  for (const QString& uri : uris) {
    fs::Location loc;
    try {
      loc = fs::Location::from_url(uri.toStdString());
    } catch (...) {
      continue;
    }
    const QString large = cache_path_for(loc, QStringLiteral("large"));
    const QString normal = cache_path_for(loc, QStringLiteral("normal"));
    if (cache_matches_source(loc, large)) {
      emit thumbnail_ready(loc, large);
    } else if (cache_matches_source(loc, normal)) {
      emit thumbnail_ready(loc, normal);
    } else if (QFileInfo::exists(large)) {
      emit thumbnail_ready(loc, large); // freshly written; trust daemon
    } else if (QFileInfo::exists(normal)) {
      emit thumbnail_ready(loc, normal);
    } else {
      emit thumbnail_failed(loc, QStringLiteral("Ready signal but cache file missing"));
    }
  }
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

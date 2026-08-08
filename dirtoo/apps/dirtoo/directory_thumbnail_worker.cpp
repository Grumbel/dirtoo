// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "directory_thumbnail_worker.hpp"

#include "dirtoo/fs/location.hpp"
#include "dirtoo/thumbnail/thumbnailer.hpp"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPainter>

#include <algorithm>
#include <array>
#include <filesystem>

namespace dirtoo::app {
namespace {

bool is_image_ext(const QString& name)
{
  const QString lower = name.toLower();
  return lower.endsWith(QLatin1String(".jpg")) || lower.endsWith(QLatin1String(".jpeg"))
         || lower.endsWith(QLatin1String(".png")) || lower.endsWith(QLatin1String(".webp"))
         || lower.endsWith(QLatin1String(".gif")) || lower.endsWith(QLatin1String(".bmp"))
         || lower.endsWith(QLatin1String(".tif")) || lower.endsWith(QLatin1String(".tiff"));
}

QImage load_scaled(const QString& path, int max_side)
{
  QImageReader reader(path);
  reader.setAutoTransform(true);
  QSize size = reader.size();
  if (size.isValid()) {
    size.scale(max_side, max_side, Qt::KeepAspectRatioByExpanding);
    reader.setScaledSize(size);
  }
  return reader.read();
}

QImage crop_center(const QImage& src, int w, int h)
{
  if (src.isNull() || w <= 0 || h <= 0) {
    return {};
  }
  const int x = std::max(0, (src.width() - w) / 2);
  const int y = std::max(0, (src.height() - h) / 2);
  return src.copy(x, y, std::min(w, src.width() - x), std::min(h, src.height() - y));
}

/// Path to an existing freedesktop thumbnail for @p file_path, or empty.
QString existing_thumb_cache(const QString& file_path)
{
  if (file_path.isEmpty()) {
    return {};
  }
  const auto loc = fs::Location::from_path(std::filesystem::path{file_path.toStdString()});
  for (const char* flavor : {"large", "normal", "x-large", "xx-large"}) {
    const QString cached =
        thumbnail::Thumbnailer::cache_path_for(loc, QString::fromLatin1(flavor));
    if (QFileInfo::exists(cached)) {
      return cached;
    }
  }
  return {};
}

/// Resolve a paintable image for one directory entry.
/// Images: decode the real file (better quality, no thumb dependency).
/// Other media (video, PDF, …): reuse an existing XDG thumbnail when present.
QImage load_tile_image(const QString& file_path, const QString& base_name, int max_side)
{
  if (is_image_ext(base_name)) {
    QImage img = load_scaled(file_path, max_side);
    if (!img.isNull()) {
      return img;
    }
    // Fall through: maybe a partial/corrupt image still has a cache entry.
  }

  const QString cached = existing_thumb_cache(file_path);
  if (cached.isEmpty()) {
    return {};
  }
  return load_scaled(cached, max_side);
}

/// Returns empty QString on success, or a human-readable failure reason.
QString build_montage(const QString& dir_path, const QString& out_path)
{
  QDir dir(dir_path);
  if (!dir.exists()) {
    return QStringLiteral("directory does not exist");
  }

  // Collect up to 9 tile sources: direct image decode and/or cached thumbs
  // (videos, PDFs, office docs, … that the D-Bus thumbnailer already produced).
  struct Tile {
    QString path;
    QString name;
  };
  QList<Tile> tiles;
  const auto entries = dir.entryList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                                     QDir::Name);

  // Pass 1: prefer real images (stable visual for photo folders).
  for (const QString& name : entries) {
    if (!is_image_ext(name)) {
      continue;
    }
    tiles.push_back({dir.absoluteFilePath(name), name});
    if (tiles.size() >= 9) {
      break;
    }
  }

  // Pass 2: fill remaining slots from non-images that already have a thumbnail
  // in the XDG cache (typical for videos). Skip files we already took as images.
  if (tiles.size() < 9) {
    for (const QString& name : entries) {
      if (is_image_ext(name)) {
        continue;
      }
      const QString abs = dir.absoluteFilePath(name);
      if (existing_thumb_cache(abs).isEmpty()) {
        continue;
      }
      tiles.push_back({abs, name});
      if (tiles.size() >= 9) {
        break;
      }
    }
  }

  if (tiles.isEmpty()) {
    return QStringLiteral(
        "no images to decode and no files with existing thumbnails to montage");
  }

  constexpr int kSize = 256;
  QImage output(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
  output.fill(QColor(40, 40, 40));

  QPainter painter(&output);
  if (!painter.isActive()) {
    return QStringLiteral("failed to create paint device for montage");
  }
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  const int n = tiles.size();
  // Normalized cell rects for 1..9 tiles (Python DirectoryThumbnailer-inspired).
  struct Cell {
    double x1, y1, x2, y2;
  };
  const Cell* cells = nullptr;
  int cell_count = n;
  static const Cell k1[] = {{0, 0, 1, 1}};
  static const Cell k2[] = {{0, 0, 0.5, 1}, {0.5, 0, 1, 1}};
  static const Cell k3[] = {{0, 0, 0.5, 1}, {0.5, 0, 1, 0.5}, {0.5, 0.5, 1, 1}};
  static const Cell k4[] = {{0, 0, 0.5, 0.5}, {0.5, 0, 1, 0.5}, {0, 0.5, 0.5, 1}, {0.5, 0.5, 1, 1}};
  static const Cell k5[] = {{0, 0, 0.333, 0.5}, {0.666, 0, 1, 0.5}, {0.333, 0, 0.666, 1},
                            {0, 0.5, 0.333, 1}, {0.666, 0.5, 1, 1}};
  static const Cell k6[] = {{0, 0, 0.333, 0.5}, {0.333, 0, 0.666, 0.5}, {0.666, 0, 1, 0.5},
                            {0, 0.5, 0.333, 1}, {0.333, 0.5, 0.666, 1}, {0.666, 0.5, 1, 1}};
  static const Cell k7[] = {{0, 0, 0.333, 0.5}, {0.333, 0, 0.666, 0.333}, {0.666, 0, 1, 0.5},
                            {0.333, 0.333, 0.666, 0.666}, {0, 0.5, 0.333, 1},
                            {0.333, 0.666, 0.666, 1}, {0.666, 0.5, 1, 1}};
  static const Cell k8[] = {{0, 0, 0.333, 0.333}, {0.333, 0, 0.666, 0.333}, {0.666, 0, 1, 0.333},
                            {0, 0.333, 0.5, 0.666}, {0.5, 0.333, 1, 0.666},
                            {0, 0.666, 0.333, 1}, {0.333, 0.666, 0.666, 1}, {0.666, 0.666, 1, 1}};
  static const Cell k9[] = {{0, 0, 0.333, 0.333}, {0.333, 0, 0.666, 0.333}, {0.666, 0, 1, 0.333},
                            {0, 0.333, 0.333, 0.666}, {0.333, 0.333, 0.666, 0.666},
                            {0.666, 0.333, 1, 0.666}, {0, 0.666, 0.333, 1},
                            {0.333, 0.666, 0.666, 1}, {0.666, 0.666, 1, 1}};
  switch (n) {
  case 1: cells = k1; break;
  case 2: cells = k2; break;
  case 3: cells = k3; break;
  case 4: cells = k4; break;
  case 5: cells = k5; break;
  case 6: cells = k6; break;
  case 7: cells = k7; break;
  case 8: cells = k8; break;
  default: cells = k9; cell_count = 9; break;
  }

  int tiles_drawn = 0;
  for (int i = 0; i < cell_count; ++i) {
    QImage img = load_tile_image(tiles[i].path, tiles[i].name, kSize);
    if (img.isNull()) {
      continue;
    }
    const auto& c = cells[i];
    QRect dst(static_cast<int>(c.x1 * kSize), static_cast<int>(c.y1 * kSize),
              static_cast<int>((c.x2 - c.x1) * kSize), static_cast<int>((c.y2 - c.y1) * kSize));
    QImage cropped = crop_center(img, dst.width(), dst.height());
    if (!cropped.isNull()) {
      painter.drawImage(dst, cropped.scaled(dst.size(), Qt::IgnoreAspectRatio,
                                            Qt::SmoothTransformation));
      ++tiles_drawn;
    }
  }
  painter.end();

  if (tiles_drawn == 0) {
    return QStringLiteral("could not load any of the %1 candidate tile source(s)").arg(tiles.size());
  }

  // Ensure cache directory exists.
  QFileInfo fi(out_path);
  if (!QDir().mkpath(fi.absolutePath())) {
    return QStringLiteral("could not create thumbnail cache directory: %1").arg(fi.absolutePath());
  }
  if (!output.save(out_path, "PNG")) {
    return QStringLiteral("failed to write montage PNG: %1").arg(out_path);
  }
  return {};
}

} // namespace

DirectoryThumbnailWorker::DirectoryThumbnailWorker(QObject* parent)
    : QObject(parent)
{
}

void DirectoryThumbnailWorker::generate(const QStringList& directory_paths)
{
  int ok = 0;
  int fail = 0;
  for (const QString& path : directory_paths) {
    if (path.isEmpty()) {
      ++fail;
      qWarning().noquote() << QStringLiteral(
          "dirtoo: directory thumbnail skipped (empty path)");
      continue;
    }
    const auto loc = fs::Location::from_path(std::filesystem::path{path.toStdString()});
    const QString out = thumbnail::Thumbnailer::cache_path_for(loc, QStringLiteral("large"));
    const QString error = build_montage(path, out);
    if (error.isEmpty()) {
      ++ok;
      emit thumbnail_ready(loc, out);
    } else {
      ++fail;
      const QString message =
          QStringLiteral("directory thumbnail failed for %1: %2").arg(path, error);
      qWarning().noquote() << message;
      emit thumbnail_failed(loc, message);
    }
  }
  emit finished(ok, fail);
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "directory_thumbnail_worker.hpp"

#include "dirtoo/thumbnail/thumbnailer.hpp"

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

bool build_montage(const QString& dir_path, const QString& out_path)
{
  QDir dir(dir_path);
  if (!dir.exists()) {
    return false;
  }

  QStringList images;
  const auto entries = dir.entryList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                                     QDir::Name);
  for (const QString& name : entries) {
    if (is_image_ext(name)) {
      images.push_back(dir.absoluteFilePath(name));
      if (images.size() >= 4) {
        break;
      }
    }
  }
  if (images.isEmpty()) {
    return false;
  }

  constexpr int kSize = 256;
  QImage output(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
  output.fill(QColor(40, 40, 40));

  QPainter painter(&output);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  const int n = images.size();
  // Layouts: 1 full, 2 side-by-side, 3 left+stack, 4 grid.
  struct Cell {
    double x1, y1, x2, y2;
  };
  std::array<std::array<Cell, 4>, 5> layouts{};
  layouts[1] = {Cell{0, 0, 1, 1}};
  layouts[2] = {Cell{0, 0, 0.5, 1}, Cell{0.5, 0, 1, 1}};
  layouts[3] = {Cell{0, 0, 0.5, 1}, Cell{0.5, 0, 1, 0.5}, Cell{0.5, 0.5, 1, 1}};
  layouts[4] = {Cell{0, 0, 0.5, 0.5}, Cell{0.5, 0, 1, 0.5}, Cell{0, 0.5, 0.5, 1},
                Cell{0.5, 0.5, 1, 1}};

  const auto& cells = layouts[static_cast<std::size_t>(n)];
  for (int i = 0; i < n; ++i) {
    QImage img = load_scaled(images[i], kSize);
    if (img.isNull()) {
      continue;
    }
    const auto& c = cells[static_cast<std::size_t>(i)];
    QRect dst(static_cast<int>(c.x1 * kSize), static_cast<int>(c.y1 * kSize),
              static_cast<int>((c.x2 - c.x1) * kSize), static_cast<int>((c.y2 - c.y1) * kSize));
    QImage cropped = crop_center(img, dst.width(), dst.height());
    if (!cropped.isNull()) {
      painter.drawImage(dst, cropped.scaled(dst.size(), Qt::IgnoreAspectRatio,
                                            Qt::SmoothTransformation));
    }
  }
  painter.end();

  // Ensure cache directory exists.
  QFileInfo fi(out_path);
  QDir().mkpath(fi.absolutePath());
  return output.save(out_path, "PNG");
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
      continue;
    }
    const auto loc = fs::Location::from_path(std::filesystem::path{path.toStdString()});
    const QString out = thumbnail::Thumbnailer::cache_path_for(loc, QStringLiteral("large"));
    if (build_montage(path, out)) {
      ++ok;
      emit thumbnail_ready(loc, out);
    } else {
      ++fail;
    }
  }
  emit finished(ok, fail);
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "directory_thumbnail_worker.hpp"
#include "mime_util.hpp"

#include "dirtoo/fs/location.hpp"
#include "dirtoo/thumbnail/thumbnailer.hpp"

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QMimeDatabase>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <array>
#include <filesystem>
#include <vector>

namespace dirtoo::app {
namespace {

constexpr int kMaxTiles = 9;
constexpr int kThumbWaitMs = 20000;
constexpr int kMaxGenerate = 12;

bool is_image_ext(const QString& name)
{
  const QString lower = name.toLower();
  return lower.endsWith(QLatin1String(".jpg")) || lower.endsWith(QLatin1String(".jpeg"))
         || lower.endsWith(QLatin1String(".png")) || lower.endsWith(QLatin1String(".webp"))
         || lower.endsWith(QLatin1String(".gif")) || lower.endsWith(QLatin1String(".bmp"))
         || lower.endsWith(QLatin1String(".tif")) || lower.endsWith(QLatin1String(".tiff"));
}

bool is_video_ext(const QString& name)
{
  const QString lower = name.toLower();
  return lower.endsWith(QLatin1String(".mp4")) || lower.endsWith(QLatin1String(".mkv"))
         || lower.endsWith(QLatin1String(".webm")) || lower.endsWith(QLatin1String(".avi"))
         || lower.endsWith(QLatin1String(".mov")) || lower.endsWith(QLatin1String(".m4v"))
         || lower.endsWith(QLatin1String(".wmv")) || lower.endsWith(QLatin1String(".flv"))
         || lower.endsWith(QLatin1String(".mpeg")) || lower.endsWith(QLatin1String(".mpg"))
         || lower.endsWith(QLatin1String(".ts")) || lower.endsWith(QLatin1String(".m2ts"));
}

bool is_document_ext(const QString& name)
{
  const QString lower = name.toLower();
  return lower.endsWith(QLatin1String(".pdf")) || lower.endsWith(QLatin1String(".odt"))
         || lower.endsWith(QLatin1String(".ods")) || lower.endsWith(QLatin1String(".odp"))
         || lower.endsWith(QLatin1String(".docx")) || lower.endsWith(QLatin1String(".xlsx"))
         || lower.endsWith(QLatin1String(".pptx"));
}

bool is_thumbnail_candidate(const QString& name)
{
  return is_image_ext(name) || is_video_ext(name) || is_document_ext(name);
}

QString existing_thumb_cache(const fs::Location& loc)
{
  for (const char* flavor : {"large", "normal", "x-large", "xx-large"}) {
    const QString cached =
        thumbnail::Thumbnailer::cache_path_for(loc, QString::fromLatin1(flavor));
    if (QFileInfo::exists(cached)) {
      return cached;
    }
  }
  return {};
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

struct Candidate {
  fs::Location location;
  QString abs_path;
  QString mime;
};

std::vector<Candidate> collect_candidates(const QString& dir_path)
{
  QDir dir(dir_path);
  const auto entries = dir.entryList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                                     QDir::Name);

  std::vector<Candidate> images;
  std::vector<Candidate> videos;
  std::vector<Candidate> docs;
  for (const QString& name : entries) {
    if (!is_thumbnail_candidate(name)) {
      continue;
    }
    const QString abs = dir.absoluteFilePath(name);
    Candidate c;
    c.abs_path = abs;
    c.location = fs::Location::from_path(std::filesystem::path{abs.toStdString()});
    c.mime = mime_for_thumbnail_fast(abs);
    if (is_image_ext(name)) {
      images.push_back(std::move(c));
    } else if (is_video_ext(name)) {
      videos.push_back(std::move(c));
    } else {
      docs.push_back(std::move(c));
    }
  }

  std::vector<Candidate> out;
  out.reserve(static_cast<std::size_t>(kMaxGenerate));
  auto append_capped = [&](std::vector<Candidate>& src) {
    for (auto& c : src) {
      if (static_cast<int>(out.size()) >= kMaxGenerate) {
        break;
      }
      out.push_back(std::move(c));
    }
  };
  append_capped(images);
  append_capped(videos);
  append_capped(docs);
  return out;
}

void ensure_child_thumbnails(const std::vector<Candidate>& candidates)
{
  std::vector<fs::Location> need;
  QStringList mimes;
  need.reserve(candidates.size());
  for (const auto& c : candidates) {
    if (!existing_thumb_cache(c.location).isEmpty()) {
      continue;
    }
    need.push_back(c.location);
    mimes.push_back(c.mime);
  }
  if (need.empty()) {
    return;
  }

  thumbnail::Thumbnailer thumb;
  int outstanding = static_cast<int>(need.size());

  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

  auto on_one = [&](const fs::Location&, const QString&) {
    if (--outstanding <= 0) {
      loop.quit();
    }
  };
  QObject::connect(&thumb, &thumbnail::Thumbnailer::thumbnail_ready, &loop, on_one);
  QObject::connect(&thumb, &thumbnail::Thumbnailer::thumbnail_failed, &loop, on_one);

  thumb.request_many(need, mimes, QStringLiteral("large"));

  if (outstanding > 0) {
    timeout.start(kThumbWaitMs);
    loop.exec();
    if (outstanding > 0) {
      qWarning().noquote() << QStringLiteral(
          "dirtoo: directory montage timed out waiting for %1 child thumbnail(s)")
                                 .arg(outstanding);
      thumb.cancel_all();
    }
  }
}

QString build_montage(const QString& dir_path, const QString& out_path)
{
  QDir dir(dir_path);
  if (!dir.exists()) {
    return QStringLiteral("directory does not exist");
  }

  auto candidates = collect_candidates(dir_path);
  if (candidates.empty()) {
    return QStringLiteral("no thumbnailable children (images/videos/documents)");
  }

  ensure_child_thumbnails(candidates);

  QStringList tile_paths;
  tile_paths.reserve(kMaxTiles);
  for (const auto& c : candidates) {
    const QString cached = existing_thumb_cache(c.location);
    if (cached.isEmpty()) {
      continue;
    }
    tile_paths.push_back(cached);
    if (tile_paths.size() >= kMaxTiles) {
      break;
    }
  }
  if (tile_paths.isEmpty()) {
    return QStringLiteral(
        "no child thumbnails available after generation attempt "
        "(%1 candidate(s); thumbnail service may be missing or slow)")
        .arg(candidates.size());
  }

  constexpr int kSize = 256;
  QImage output(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
  output.fill(QColor(40, 40, 40));

  QPainter painter(&output);
  if (!painter.isActive()) {
    return QStringLiteral("failed to create paint device for montage");
  }
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  const int n = tile_paths.size();
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
    QImage img = load_scaled(tile_paths[i], kSize);
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
    return QStringLiteral("could not decode any of the %1 child thumbnail file(s)")
        .arg(tile_paths.size());
  }

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

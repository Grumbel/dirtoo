// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "directory_load_worker.hpp"

#include <exception>
#include <filesystem>
#include <system_error>

namespace dirtoo::app {

DirectoryLoadWorker::DirectoryLoadWorker(QObject* parent)
    : QObject(parent)
{
}

void DirectoryLoadWorker::cancel()
{
  // Invalidate whatever generation is in flight (including generation 0).
  const quint64 cur = cancel_generation_.load(std::memory_order_relaxed);
  cancel_generation_.store(cur ^ ~quint64{0}, std::memory_order_relaxed);
}

void DirectoryLoadWorker::load(const QString& path, quint64 generation)
{
  cancel_generation_.store(generation, std::memory_order_relaxed);

  try {
    const auto dir_path = std::filesystem::path{path.toStdString()};
    std::vector<fs::FileInfo> items;
    items.reserve(256);
    std::error_code ec;
    const auto opts = std::filesystem::directory_options::skip_permission_denied;
    int seen = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir_path, opts, ec)) {
      if (ec) {
        ec.clear();
        continue;
      }
      if (cancel_generation_.load(std::memory_order_relaxed) != generation) {
        return; // superseded
      }
      items.push_back(fs::FileInfo::from_directory_entry(entry));
      ++seen;
      // Keep the UI informed on slow media without flooding the event queue.
      if ((seen % 32) == 0) {
        emit progress(generation, seen);
      }
    }
    if (cancel_generation_.load(std::memory_order_relaxed) != generation) {
      return;
    }
    emit loaded(generation, std::move(items));
  } catch (const std::exception& ex) {
    if (cancel_generation_.load(std::memory_order_relaxed) != generation) {
      return;
    }
    emit failed(generation, QString::fromUtf8(ex.what()));
  } catch (...) {
    if (cancel_generation_.load(std::memory_order_relaxed) != generation) {
      return;
    }
    emit failed(generation, QStringLiteral("Failed to list directory"));
  }
}

} // namespace dirtoo::app

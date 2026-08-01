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
  // Invalidate any in-flight load; load() bails when generation no longer matches.
  cancel_generation_.fetch_add(1, std::memory_order_relaxed);
}

void DirectoryLoadWorker::load(const QString& path, quint64 generation)
{
  // Track the generation this call is serving; cancel()/newer loads may supersede it.
  cancel_generation_.store(generation, std::memory_order_relaxed);

  try {
    const auto dir_path = std::filesystem::path{path.toStdString()};
    std::vector<fs::FileInfo> items;
    items.reserve(256);
    std::error_code ec;
    const auto opts = std::filesystem::directory_options::skip_permission_denied;
    for (const auto& entry : std::filesystem::directory_iterator(dir_path, opts, ec)) {
      if (ec) {
        ec.clear();
        continue;
      }
      if (cancel_generation_.load(std::memory_order_relaxed) != generation) {
        return; // superseded by a newer load or cancel()
      }
      items.push_back(fs::FileInfo::from_directory_entry(entry));
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

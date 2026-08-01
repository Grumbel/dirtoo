// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "directory_load_worker.hpp"

#include <exception>

namespace dirtoo::app {

DirectoryLoadWorker::DirectoryLoadWorker(QObject* parent)
    : QObject(parent)
{
}

void DirectoryLoadWorker::load(const QString& path, quint64 generation)
{
  try {
    const auto loc = fs::Location::from_path(std::filesystem::path{path.toStdString()});
    std::vector<fs::FileInfo> items = fs::list_directory(loc);
    emit loaded(generation, std::move(items));
  } catch (const std::exception& ex) {
    emit failed(generation, QString::fromUtf8(ex.what()));
  } catch (...) {
    emit failed(generation, QStringLiteral("Failed to list directory"));
  }
}

} // namespace dirtoo::app

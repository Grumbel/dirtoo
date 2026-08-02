// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "archive_member_cache.hpp"

#include "dirtoo/archive/archive_index.hpp"

#include <QStandardPaths>
#include <QString>

#include <string>

namespace dirtoo::app {

std::filesystem::path archive_member_cache_root(std::string_view subdir)
{
  const QString base =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
      + QLatin1Char('/')
      + QString::fromUtf8(subdir.data(), static_cast<int>(subdir.size()));
  return std::filesystem::path{base.toStdString()};
}

std::filesystem::path archive_member_dest_dir(const std::filesystem::path& cache_root,
                                              const std::filesystem::path& archive_file)
{
  return cache_root / std::to_string(std::hash<std::string>{}(archive_file.string()));
}

std::optional<std::filesystem::path>
ensure_archive_member_extracted(const std::filesystem::path& archive_file,
                                const std::filesystem::path& member,
                                const std::filesystem::path& dest_dir)
{
  const auto cached = dest_dir / member;
  std::error_code ec;
  if (std::filesystem::is_regular_file(cached, ec) && !ec) {
    return cached;
  }
  auto extracted = archive::extract_member(archive_file, member, dest_dir);
  if (extracted) {
    return *extracted;
  }
  return std::nullopt;
}

} // namespace dirtoo::app

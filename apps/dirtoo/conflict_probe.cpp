// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "conflict_probe.hpp"

#include <system_error>

namespace dirtoo::app {
namespace {

ConflictEntryInfo probe_entry(const std::filesystem::path& path)
{
  ConflictEntryInfo info;
  if (path.empty()) {
    return info;
  }
  std::error_code ec;
  const auto st = std::filesystem::status(path, ec);
  if (ec || !std::filesystem::exists(st)) {
    return info;
  }
  info.exists = true;
  info.is_directory = std::filesystem::is_directory(st);
  if (!info.is_directory) {
    const auto sz = std::filesystem::file_size(path, ec);
    if (!ec) {
      info.size = static_cast<std::uint64_t>(sz);
    }
  }
  const auto mt = std::filesystem::last_write_time(path, ec);
  if (!ec) {
    info.mtime = mt;
  }
  return info;
}

} // namespace

ConflictProbe probe_conflict(const std::filesystem::path& source,
                             const std::filesystem::path& destination)
{
  return ConflictProbe{probe_entry(source), probe_entry(destination)};
}

} // namespace dirtoo::app

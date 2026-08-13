// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mutation_support.hpp"

#include <QApplication>
#include <QClipboard>

namespace dirtoo::app {

QString clipboard_mode_verb(ClipboardMode mode)
{
  switch (mode) {
  case ClipboardMode::Cut:
    return QStringLiteral("cut");
  case ClipboardMode::Link:
    return QStringLiteral("marked for link");
  case ClipboardMode::Copy:
  default:
    return QStringLiteral("copied");
  }
}

std::vector<std::filesystem::path> paths_from_fileinfos(const std::vector<fs::FileInfo>& files)
{
  std::vector<std::filesystem::path> paths;
  paths.reserve(files.size());
  for (const auto& fi : files) {
    paths.push_back(fi.path());
  }
  return paths;
}

bool location_allows_filesystem_mutations(const fs::Location& location)
{
  return !location.is_archive() && !location.is_tag();
}

void apply_paths_to_system_clipboard(ClipboardMode mode,
                                     const std::vector<std::filesystem::path>& paths)
{
  QApplication::clipboard()->setMimeData(make_clipboard_mime(mode, paths));
}

} // namespace dirtoo::app

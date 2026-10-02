// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"

#include <QIcon>

#include <filesystem>

namespace dirtoo::app {

/// Theme icon for an entry from its name and known type only — no disk I/O.
///
/// Use instead of QFileIconProvider::icon(QFileInfo): that stats the path and
/// may sniff file content via QMimeDatabase::MatchDefault, which freezes the
/// GUI when the entry lives on a slow USB/network drive. GUI thread only
/// (icons are cached per MIME type).
[[nodiscard]] QIcon file_type_icon(const std::filesystem::path& path, bool is_directory);

/// Same, using the type captured in @p fi (symlinks to directories get the
/// folder icon).
[[nodiscard]] QIcon file_type_icon(const fs::FileInfo& fi);

} // namespace dirtoo::app

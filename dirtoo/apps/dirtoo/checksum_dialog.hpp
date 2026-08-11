// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStringList>
#include <QWidget>

#include <filesystem>
#include <vector>

namespace dirtoo::app {

/// Show checksum dialog for the given paths (compute/refresh via ChecksumStore).
/// Hashing runs off the GUI thread.
void show_checksum_dialog(QWidget* parent, const std::vector<std::filesystem::path>& paths);

/// Convenience: open dialog for a QStringList of local paths.
void show_checksum_dialog(QWidget* parent, const QStringList& paths);

} // namespace dirtoo::app

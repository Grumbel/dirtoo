// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QWidget>

#include <filesystem>
#include <vector>

namespace dirtoo::app {

/// Open path with the desktop default application.
bool open_default(const std::filesystem::path& path);

/// Open directory in a terminal emulator (xdg heuristics).
bool open_in_terminal(const std::filesystem::path& directory);

/// Prompt for a command and run it with the given paths as arguments.
bool open_with_command_dialog(QWidget* parent, const std::vector<std::filesystem::path>& paths);

} // namespace dirtoo::app

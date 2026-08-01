// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

#include <filesystem>
#include <vector>

class QMenu;

namespace dirtoo::app {

struct DesktopApp {
  QString id;   // e.g. org.gnome.gedit.desktop
  QString name; // human name
  QString exec; // Exec= line (may contain %f/%F/%u/%U)
  QString icon; // Icon= name
};

/// Open path with the desktop default application.
bool open_default(const std::filesystem::path& path);

/// Open directory in a terminal emulator (xdg heuristics).
bool open_in_terminal(const std::filesystem::path& directory);

/// Prompt for a command and run it with the given paths as arguments.
bool open_with_command_dialog(QWidget* parent, const std::vector<std::filesystem::path>& paths);

/// Default + associated apps for a MIME type (from mimeapps.list + desktop files).
[[nodiscard]] std::vector<DesktopApp> apps_for_mime(const QString& mime_type);

/// Launch a desktop app with the given local paths (substitutes %f/%F/%u/%U).
bool launch_desktop_app(const DesktopApp& app, const std::vector<std::filesystem::path>& paths);

/// Populate an "Open with" submenu for the given paths (shared MIME intersection).
void populate_open_with_menu(QMenu* menu, const std::vector<std::filesystem::path>& paths);

} // namespace dirtoo::app

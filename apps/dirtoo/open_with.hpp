// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"

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
/// A path plus the type already known from the listing, so MIME lookup for
/// application menus never has to stat or read the file on the GUI thread.
struct OpenTarget {
  std::filesystem::path path;
  bool is_directory = false;
};

[[nodiscard]] std::vector<OpenTarget> open_targets_from(const std::vector<fs::FileInfo>& files);
/// For paths of unknown type (e.g. history entries): treated as files.
[[nodiscard]] std::vector<OpenTarget>
open_targets_from(const std::vector<std::filesystem::path>& paths);

bool open_default(const OpenTarget& target);

/// Open directory in a terminal emulator (xdg heuristics).
bool open_in_terminal(const std::filesystem::path& directory);

/// Prompt for a command and run it with the given paths as arguments.
bool open_with_command_dialog(QWidget* parent, const std::vector<std::filesystem::path>& paths);

/// Default applications for a MIME type ([Default Applications] in mimeapps.list).
[[nodiscard]] std::vector<DesktopApp> default_apps_for_mime(const QString& mime_type);

/// All associated apps (defaults + Added Associations + MIME Cache + desktop MimeType=).
[[nodiscard]] std::vector<DesktopApp> apps_for_mime(const QString& mime_type);

/// Launch a desktop app with the given local paths (substitutes %f/%F/%u/%U).
bool launch_desktop_app(const DesktopApp& app, const std::vector<std::filesystem::path>& paths);

/// Intersection of default apps across selected paths' MIME types.
[[nodiscard]] std::vector<DesktopApp>
default_apps_for_paths(const std::vector<OpenTarget>& targets);

/// Intersection of all associated apps across selected paths' MIME types.
[[nodiscard]] std::vector<DesktopApp>
associated_apps_for_paths(const std::vector<OpenTarget>& targets);

/// Add top-level "Open With <App>" actions for default handlers (Python ItemContextMenu).
void add_default_open_actions(QMenu* menu, const std::vector<OpenTarget>& targets);

/// Populate an "Open with…" submenu (other apps + custom command).
void populate_open_with_menu(QMenu* menu, const std::vector<OpenTarget>& targets);

} // namespace dirtoo::app

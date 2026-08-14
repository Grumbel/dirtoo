// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/fs/location.hpp"

#include <QPoint>
#include <QWidget>

#include <filesystem>
#include <functional>
#include <vector>

namespace dirtoo::app {

/// Callbacks for background (directory) and item context menus.
/// Keeps menu construction out of MainWindow (S3 factoring).
struct FileContextMenuCallbacks {
  fs::Location current_location;

  std::function<void()> mkdir;
  std::function<void()> create_file;
  std::function<void()> paste;
  std::function<void()> select_all;
  std::function<void()> cut;
  std::function<void()> copy;
  std::function<void()> delete_selected;
  std::function<void()> rename_selected;
  std::function<void()> properties_selected;
  std::function<void()> checksums_selected;
  std::function<void()> tag_selected;
  std::function<void()> create_set_from_selection; // Ctrl+G toggle semantics
  std::function<void()> create_new_set_from_selection;
  std::function<void()> add_selection_to_last_set;
  std::function<void()> remove_selection_from_set;
  std::function<void()> select_set_members;
  std::function<void()> open_set_of_selection;
  std::function<void()> mark_opened;
  std::function<void()> mark_unopened;
  std::function<void()> reload_thumbnails;
  std::function<void()> prepare_thumbnails;
  std::function<void()> make_directory_thumbnails;

  std::function<void(const fs::Location&)> open_location;
  std::function<void(const fs::Location&)> open_location_new_window;
  std::function<void(const std::filesystem::path&)> open_terminal;
  std::function<void(const std::filesystem::path& dest_dir)> paste_into;
  std::function<void(const QString&)> set_status;
  std::function<void(const std::vector<fs::FileInfo>&)> show_properties;
};

/// Build and exec the background (empty area) context menu.
void exec_directory_context_menu(QWidget* parent, const QPoint& global_pos,
                                 const FileContextMenuCallbacks& cb);

/// Build and exec the item context menu for the current selection.
void exec_item_context_menu(QWidget* parent, const QPoint& global_pos,
                            const std::vector<fs::FileInfo>& selected,
                            const FileContextMenuCallbacks& cb);

} // namespace dirtoo::app

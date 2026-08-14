// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "file_context_menu.hpp"

#include "open_with.hpp"
#include "theme_icons.hpp"

#include "dirtoo/fs/location.hpp"

#include <filesystem>

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QStringList>

namespace dirtoo::app {

void exec_directory_context_menu(QWidget* parent, const QPoint& global_pos,
                                 const FileContextMenuCallbacks& cb)
{
  QMenu menu(parent);
  if (cb.mkdir) {
    menu.addAction(theme_icon("folder-new"), QStringLiteral("Create Directory…"), parent, cb.mkdir);
  }
  if (cb.create_file) {
    menu.addAction(theme_icon("document-new"), QStringLiteral("Create Empty File…"), parent,
                   cb.create_file);
  }
  menu.addSeparator();
  if (cb.paste) {
    menu.addAction(theme_icon("edit-paste"), QStringLiteral("Paste"), parent, cb.paste);
  }
  menu.addSeparator();
  if (cb.open_terminal) {
    const auto loc = cb.current_location;
    menu.addAction(theme_icon("utilities-terminal"), QStringLiteral("Open Terminal Here"), parent,
                   [cb, loc] { cb.open_terminal(loc.as_path()); });
  }
  menu.addSeparator();
  if (cb.select_all) {
    menu.addAction(theme_icon("edit-select-all"), QStringLiteral("Select All"), parent, cb.select_all);
  }
  menu.addSeparator();
  if (cb.show_properties) {
    const auto loc = cb.current_location;
    menu.addAction(theme_icon("document-properties"), QStringLiteral("Properties…"), parent,
                   [cb, loc] {
                     std::vector<fs::FileInfo> items;
                     items.push_back(fs::FileInfo::from_path(loc.as_path()));
                     cb.show_properties(items);
                   });
  }
  menu.exec(global_pos);
}

void exec_item_context_menu(QWidget* parent, const QPoint& global_pos,
                            const std::vector<fs::FileInfo>& selected,
                            const FileContextMenuCallbacks& cb)
{
  QMenu menu(parent);

  std::vector<std::filesystem::path> paths;
  paths.reserve(selected.size());
  for (const auto& fi : selected) {
    paths.push_back(fi.path());
  }

  const bool single = selected.size() == 1;
  const fs::FileInfo* primary = single ? &selected.front() : nullptr;

  if (primary != nullptr && primary->is_directory()) {
    const fs::Location open_loc = primary->location();
    if (cb.open_location) {
      menu.addAction(theme_icon("folder"), QStringLiteral("Open Folder"), parent,
                     [cb, open_loc] { cb.open_location(open_loc); });
    }
    if (cb.open_location_new_window) {
      menu.addAction(theme_icon("window-new"), QStringLiteral("Open Folder in New Window"), parent,
                     [cb, open_loc] { cb.open_location_new_window(open_loc); });
    }
    menu.addSeparator();
  } else if (primary != nullptr && primary->is_symlink()) {
    std::error_code ec;
    const auto target = std::filesystem::read_symlink(primary->path(), ec);
    if (!ec) {
      std::filesystem::path resolved = primary->path().parent_path() / target;
      if (target.is_absolute()) {
        resolved = target;
      }
      std::error_code ec2;
      const auto st = std::filesystem::status(resolved, ec2);
      if (!ec2 && std::filesystem::is_directory(st) && cb.open_location) {
        const fs::Location tloc = fs::Location::from_path(resolved);
        menu.addAction(theme_icon("go-jump"), QStringLiteral("Open Link Target"), parent,
                       [cb, tloc] { cb.open_location(tloc); });
        if (cb.open_location_new_window) {
          menu.addAction(theme_icon("window-new"), QStringLiteral("Open Link Target in New Window"),
                         parent, [cb, tloc] { cb.open_location_new_window(tloc); });
        }
        menu.addSeparator();
      } else if (!ec2 && cb.open_location) {
        // File target: open containing directory.
        const fs::Location parent_loc = fs::Location::from_path(resolved.parent_path());
        menu.addAction(theme_icon("folder"), QStringLiteral("Open Target's Folder"), parent,
                       [cb, parent_loc] { cb.open_location(parent_loc); });
        menu.addSeparator();
      }
    }
  } else if (primary != nullptr && fs::looks_like_archive(primary->path())
             && !cb.current_location.is_archive()) {
    const std::filesystem::path archive_path = primary->path();
    if (cb.open_location) {
      menu.addAction(theme_icon("package-x-generic"), QStringLiteral("Open Archive"), parent,
                     [cb, archive_path] {
                       cb.open_location(fs::Location::from_archive(archive_path, {}));
                     });
    }
    menu.addSeparator();
  }

  add_default_open_actions(&menu, paths);
  {
    auto* open_with = menu.addMenu(theme_icon("system-run"), QStringLiteral("Open with…"));
    populate_open_with_menu(open_with, paths);
  }
  menu.addSeparator();

  if (primary != nullptr && cb.open_location) {
    const fs::Location parent_loc = primary->location().parent();
    menu.addAction(theme_icon("folder"), QStringLiteral("Open Containing Folder"), parent,
                   [cb, parent_loc] { cb.open_location(parent_loc); });
    menu.addSeparator();
  }

  if (primary != nullptr && primary->is_directory() && cb.open_terminal) {
    const std::filesystem::path dir = primary->path();
    menu.addAction(theme_icon("utilities-terminal"), QStringLiteral("Open Terminal Here"), parent,
                   [cb, dir] { cb.open_terminal(dir); });
    menu.addSeparator();
  }

  if (cb.cut) {
    menu.addAction(theme_icon("edit-cut"), QStringLiteral("Cut"), parent, cb.cut);
  }
  if (cb.copy) {
    menu.addAction(theme_icon("edit-copy"), QStringLiteral("Copy"), parent, cb.copy);
  }
  {
    auto* paste_into =
        menu.addAction(theme_icon("edit-paste"), QStringLiteral("Paste Into Folder"));
    const bool can_paste_into = primary != nullptr && primary->is_directory() && cb.paste_into;
    paste_into->setEnabled(can_paste_into);
    if (can_paste_into) {
      const std::filesystem::path dest = primary->path();
      QObject::connect(paste_into, &QAction::triggered, parent, [cb, dest] { cb.paste_into(dest); });
    }
  }
  menu.addAction(theme_icon("edit-copy"), QStringLiteral("Copy Path"), parent, [cb, paths] {
    QStringList lines;
    for (const auto& p : paths) {
      lines << QString::fromStdString(p.string());
    }
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    if (cb.set_status) {
      cb.set_status(QStringLiteral("Copied %1 path(s)").arg(lines.size()));
    }
  });
  menu.addSeparator();
  if (cb.delete_selected) {
    menu.addAction(theme_icon("edit-delete"), QStringLiteral("Delete…"), parent, cb.delete_selected);
  }
  menu.addSeparator();
  if (cb.rename_selected) {
    menu.addAction(theme_icon("edit-rename", "document-edit"), QStringLiteral("Rename…"), parent,
                   cb.rename_selected);
  }
  menu.addSeparator();

  auto* actions = menu.addMenu(QStringLiteral("Actions"));
  if (cb.reload_thumbnails) {
    actions->addAction(theme_icon("view-refresh"), QStringLiteral("Reload Thumbnails"), parent,
                       cb.reload_thumbnails);
  }
  if (cb.prepare_thumbnails) {
    actions->addAction(theme_icon("image-x-generic"), QStringLiteral("Prepare Thumbnails"), parent,
                       cb.prepare_thumbnails);
  }
  if (cb.make_directory_thumbnails) {
    actions->addAction(theme_icon("folder"), QStringLiteral("Make Directory Thumbnails"), parent,
                       cb.make_directory_thumbnails);
  }

  menu.addSeparator();
  if (cb.properties_selected) {
    menu.addAction(theme_icon("document-properties"), QStringLiteral("Properties…"), parent,
                   cb.properties_selected);
  }
  if (cb.tag_selected) {
    if (cb.create_set_from_selection) {
    menu.addAction(theme_icon("object-group", "folder-new"), QStringLiteral("Create Set"), parent,
                   [cb] { cb.create_set_from_selection(); });
  }
  if (cb.add_selection_to_last_set) {
    menu.addAction(theme_icon("list-add", "folder-new"), QStringLiteral("Add to Last Set"), parent,
                   [cb] { cb.add_selection_to_last_set(); });
  }
  if (cb.select_set_members) {
    menu.addAction(theme_icon("edit-select-all", "object-group"), QStringLiteral("Select Set Members"), parent,
                   [cb] { cb.select_set_members(); });
  }
  if (cb.open_set_of_selection) {
    menu.addAction(theme_icon("folder-open", "object-group"), QStringLiteral("Show Set"), parent,
                   [cb] { cb.open_set_of_selection(); });
  }
  menu.addAction(theme_icon("bookmark-new", "tag"), QStringLiteral("Tag…"), parent,
                   cb.tag_selected);
  }
  if (cb.mark_opened) {
    menu.addAction(theme_icon("mail-mark-read", "checkmark"), QStringLiteral("Mark as Opened"),
                   parent, cb.mark_opened);
  }
  if (cb.mark_unopened) {
    menu.addAction(theme_icon("mail-mark-unread", "edit-clear"), QStringLiteral("Mark as Unopened"),
                   parent, cb.mark_unopened);
  }
  if (cb.checksums_selected) {
    menu.addAction(theme_icon("document-properties"), QStringLiteral("Checksums…"), parent,
                   cb.checksums_selected);
  }

  menu.exec(global_pos);
}

} // namespace dirtoo::app

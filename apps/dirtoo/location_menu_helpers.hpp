// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "history_menu.hpp"
#include "location_icons.hpp"
#include "theme_icons.hpp"

#include "dirtoo/fs/location.hpp"

#include <QAction>
#include <QApplication>
#include <QString>

#include <functional>
#include <vector>

namespace dirtoo::app {

/// Label for a location in history/bookmark menus.
[[nodiscard]] inline QString location_menu_label(const fs::Location& loc)
{
  return loc.is_archive() ? QString::fromStdString(loc.as_url())
                          : QString::fromStdString(loc.as_path().string());
}

/// Add location entries that open on click / new window on middle-click or Shift.
inline void add_location_menu_entries(
    HistoryMenu* menu,
    const std::vector<fs::Location>& locations,
    QObject* context,
    const std::function<void(const fs::Location&)>& open,
    const std::function<void(const fs::Location&)>& open_new_window)
{
  if (menu == nullptr) {
    return;
  }
  for (const auto& loc : locations) {
    auto* act = menu->addAction(icon_for_location(loc), location_menu_label(loc));
    QObject::connect(act, &QAction::triggered, context, [menu, loc, open, open_new_window] {
      if (menu != nullptr && menu->middle_pressed()) {
        if (open_new_window) {
          open_new_window(loc);
        }
      } else if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
        if (open_new_window) {
          open_new_window(loc);
        }
      } else if (open) {
        open(loc);
      }
    });
  }
}

} // namespace dirtoo::app

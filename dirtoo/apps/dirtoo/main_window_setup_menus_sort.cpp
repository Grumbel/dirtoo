// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include "badge_icons.hpp"
#include "location_icons.hpp"
#include "location_url.hpp"
#include "location_menu_helpers.hpp"
#include "file_views.hpp"
#include "file_context_menu.hpp"
#include "file_item_delegate.hpp"
#include "devices_controller.hpp"
#include "udisks_client.hpp"
#include "about_dialog.hpp"
#include "open_history.hpp"
#include "operations_history.hpp"
#include "preferences_dialog.hpp"
#include "open_with.hpp"
#include <QActionGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenuBar>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QKeySequence>
#include <QPushButton>

namespace dirtoo::app {

void MainWindow::setup_sort_menu()
{
  auto* sort_menu = menuBar()->addMenu(QStringLiteral("&Sort"));
  auto* sort_group = new QActionGroup(this);
  sort_group->setExclusive(true);
  auto add_sort = [&](const QString& title, dirtoo::collection::SortKey key, bool checked = false) {
    auto* act = sort_menu->addAction(title);
    act->setCheckable(true);
    act->setChecked(checked);
    sort_group->addAction(act);
    connect(act, &QAction::triggered, this, [this, key] {
      apply_sort_key(key, /*toggle_if_same=*/false);
    });
    return act;
  };
  add_sort(QStringLiteral("By Name"), dirtoo::collection::SortKey::Name, true);
  add_sort(QStringLiteral("By Size"), dirtoo::collection::SortKey::Size);
  add_sort(QStringLiteral("By Extension"), dirtoo::collection::SortKey::Extension);
  add_sort(QStringLiteral("By Date"), dirtoo::collection::SortKey::Modified);
  add_sort(QStringLiteral("By Type"), dirtoo::collection::SortKey::Type);
  sort_menu->addSeparator();
  add_sort(QStringLiteral("By Width"), dirtoo::collection::SortKey::Width);
  add_sort(QStringLiteral("By Height"), dirtoo::collection::SortKey::Height);
  add_sort(QStringLiteral("By Resolution"), dirtoo::collection::SortKey::Resolution);
  add_sort(QStringLiteral("By Aspect Ratio"), dirtoo::collection::SortKey::AspectRatio);
  add_sort(QStringLiteral("By Duration"), dirtoo::collection::SortKey::Duration);
  add_sort(QStringLiteral("By Framerate"), dirtoo::collection::SortKey::Framerate);
  sort_menu->addSeparator();
  add_sort(QStringLiteral("By Permissions"), dirtoo::collection::SortKey::Permissions);
  add_sort(QStringLiteral("Random Shuffle"), dirtoo::collection::SortKey::Random);
  sort_menu->addSeparator();
  {
    auto* act = sort_menu->addAction(theme_icon("folder"), QStringLiteral("Directories First"));
    act->setCheckable(true);
    act->setChecked(true);
    connect(act, &QAction::toggled, this, [this](bool on) {
      collection_.sorter().set_directories_first(on);
      {
        AppSettings s = load_settings();
        s.directories_first = on;
        save_settings(s);
      }
      request_async_sort();
    });
  }
  {
    auto* act = sort_menu->addAction(QStringLiteral("Reverse Order"));
    act->setCheckable(true);
    connect(act, &QAction::toggled, this, [this](bool on) {
      sort_ascending_ = !on;
      collection_.sorter().set_ascending(!on);
      request_async_sort();
    });
  }

}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"
#include "history_menu.hpp"

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

void MainWindow::setup_go_help_menus()
{
  auto* go_menu = menuBar()->addMenu(QStringLiteral("&Go"));
  go_menu->addAction(theme_icon("go-previous"), QStringLiteral("Back"), this, &MainWindow::on_go_back);
  go_menu->addAction(theme_icon("go-next"), QStringLiteral("Forward"), this, &MainWindow::on_go_forward);
  go_menu->addAction(theme_icon("go-up"), QStringLiteral("Parent"), this, &MainWindow::on_go_parent);
  go_menu->addAction(theme_icon("window-new"), QStringLiteral("Parent in New Window"), this, &MainWindow::on_parent_new_window);
  go_menu->addAction(theme_icon("go-home"), QStringLiteral("Home"), this, &MainWindow::on_go_home);

  bookmarks_menu_ = new HistoryMenu(QStringLiteral("&Bookmarks"), this);
  menuBar()->addMenu(bookmarks_menu_);
  connect(bookmarks_menu_, &QMenu::aboutToShow, this, &MainWindow::on_rebuild_bookmarks_menu);

  history_menu_ = new HistoryMenu(QStringLiteral("H&istory"), this);
  menuBar()->addMenu(history_menu_);
  connect(history_menu_, &QMenu::aboutToShow, this, &MainWindow::on_rebuild_history_menu);

  recent_opens_menu_ = menuBar()->addMenu(QStringLiteral("Recently &Opened"));
  connect(recent_opens_menu_, &QMenu::aboutToShow, this, &MainWindow::on_rebuild_recent_opens_menu);

  auto* tools_menu = menuBar()->addMenu(QStringLiteral("&Tools"));
  tools_menu->addAction(theme_icon("document-properties"), QStringLiteral("Checksums…"),
                        this, &MainWindow::on_checksums);
  {
    auto* act = tools_menu->addAction(theme_icon("bookmark-new", "tag"), QStringLiteral("Tag…"),
                                      this, &MainWindow::on_tag_selected);
    act->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
    act->setStatusTip(QStringLiteral("Tag the selected regular file(s)"));
  }

  auto* help_menu = menuBar()->addMenu(QStringLiteral("&Help"));
  help_menu->addAction(theme_icon("help-contents"), QStringLiteral("Filter expression help"), this,
                       &MainWindow::on_show_filter_help);
  help_menu->addAction(theme_icon("help-about"), QStringLiteral("About dirtoo"), this, &MainWindow::on_about);
}

} // namespace dirtoo::app

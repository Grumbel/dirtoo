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

void MainWindow::setup_edit_menu()
{
  auto* edit_menu = menuBar()->addMenu(QStringLiteral("&Edit"));
  {
    auto* act = edit_menu->addAction(theme_icon("edit-cut"), QStringLiteral("Cut"), this, &MainWindow::on_cut);
    act->setShortcut(QKeySequence::Cut);
  }
  {
    auto* act = edit_menu->addAction(theme_icon("edit-copy"), QStringLiteral("Copy"), this, &MainWindow::on_copy);
    act->setShortcut(QKeySequence::Copy);
  }
  {
    paste_act_ = edit_menu->addAction(theme_icon("edit-paste"), QStringLiteral("Paste"), this, &MainWindow::on_paste);
    paste_act_->setShortcut(QKeySequence::Paste);
  }
  edit_menu->addAction(theme_icon("emblem-symbolic-link", "insert-link"), QStringLiteral("Paste as Link"), this, &MainWindow::on_paste_link);
  edit_menu->addAction(theme_icon("edit-copy"), QStringLiteral("Copy as Link"), this, [this] {
    set_clipboard(ClipboardMode::Link);
  });
  edit_menu->addSeparator();
  edit_menu->addAction(theme_icon("view-history", "document-open-recent"),
                       QStringLiteral("Operations History…"), this, [this] {
                         show_operations_history_dialog(this, [this](const QString& dir) {
                           open_location(fs::Location::from_path(dir.toStdString()));
                         });
                       });
  {
    auto* act = edit_menu->addAction(theme_icon("edit-select-all"), QStringLiteral("Select All"), this, &MainWindow::on_select_all);
    act->setShortcut(QKeySequence::SelectAll);
  }
  edit_menu->addSeparator();
  {
    auto* act = edit_menu->addAction(theme_icon("edit-rename", "document-edit"), QStringLiteral("Rename…"), this, &MainWindow::on_rename_selected);
    act->setShortcut(QKeySequence(Qt::Key_F2));
  }
  {
    auto* act = edit_menu->addAction(theme_icon("edit-delete"), QStringLiteral("Delete…"), this, &MainWindow::on_delete_selected);
    act->setShortcut(QKeySequence::Delete);
  }
  {
    auto* act = edit_menu->addAction(theme_icon("object-flip-horizontal", "edit-copy"), QStringLiteral("Swap Names"), this, &MainWindow::on_swap_names);
    act->setStatusTip(QStringLiteral("Exchange the names of exactly two selected items"));
  }
  {
    auto* act = edit_menu->addAction(theme_icon("document-properties"), QStringLiteral("Properties…"), this, &MainWindow::on_properties);
    act->setShortcut(QKeySequence(Qt::Key_F3));
  }
  edit_menu->addSeparator();
  {
    auto* act = edit_menu->addAction(theme_icon("preferences-system"), QStringLiteral("Preferences…"), this, &MainWindow::on_preferences);
    act->setShortcut(QKeySequence::Preferences);
  }

}

} // namespace dirtoo::app

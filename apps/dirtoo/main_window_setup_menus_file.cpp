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

void MainWindow::setup_file_menu()
{
  auto* file_menu = menuBar()->addMenu(QStringLiteral("&File"));
  {
    auto* act = file_menu->addAction(theme_icon("window-new"), QStringLiteral("New Window"), this, &MainWindow::on_new_window);
    act->setShortcut(QKeySequence::New); // Ctrl+N
  }
  file_menu->addAction(theme_icon("folder-new"), QStringLiteral("New Folder…"), this, &MainWindow::on_mkdir);
  file_menu->addAction(theme_icon("document-new"), QStringLiteral("New File…"), this, &MainWindow::on_create_file);
  file_menu->addSeparator();
  {
    auto* act = file_menu->addAction(theme_icon("document-save-as"), QStringLiteral("Save File List As…"), this,
                                     &MainWindow::on_save_file_list);
    act->setShortcut(QKeySequence::SaveAs);
  }
  file_menu->addSeparator();
  {
    auto* act = file_menu->addAction(theme_icon("window-close"), QStringLiteral("Close"), this, &QWidget::close);
    act->setShortcut(QKeySequence::Close);
  }

}

} // namespace dirtoo::app

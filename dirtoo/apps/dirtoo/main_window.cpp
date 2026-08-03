// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"
#include <QThread>
#include "badge_icons.hpp"
#include "theme_icons.hpp"
#include "location_icons.hpp"
#include "location_url.hpp"
#include "location_menu_helpers.hpp"
#include "directory_tree_model.hpp"
#include "udisks_client.hpp"
#include "devices_controller.hpp"
#include "filter_worker.hpp"
#include "directory_thumbnail_worker.hpp"
#include "file_views.hpp"
#include "file_context_menu.hpp"
#include "file_list_model.hpp"
#include "graphics_file_view.hpp"
#include "graphics_file_item.hpp"
#include "file_item_delegate.hpp"
#include "dirtoo/filter/media_meta_cache.hpp"

#include "dirtoo/filter/parser.hpp"

#include "clipboard.hpp"
#include "about_dialog.hpp"
#include "name_input_dialog.hpp"
#include "app_settings.hpp"
#include "size_format.hpp"
#include "conflict_dialog.hpp"
#include "open_with.hpp"
#include "open_history.hpp"
#include "operations_history.hpp"
#include "preferences_dialog.hpp"
#include "properties_dialog.hpp"
#include "archive_member_cache.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/thumbnail/thumbnailer.hpp"
#include "dirops/ops.hpp"
#include "dirops/util.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QtConcurrent>
#include "dirtoo/archive/archive_index.hpp"
#include <QApplication>
#include <QDateTime>
#include <QCoreApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QShowEvent>
#include <QCompleter>
#include <QStringListModel>
#include <QTimer>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QFrame>
#include <QFileDialog>
#include <QTextStream>
#include <QMouseEvent>
#include <QDesktopServices>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <set>
#include <QHeaderView>
#include <QHash>
#include <QIcon>
#include <QPainter>
#include <QSvgRenderer>
#include <QKeyEvent>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMimeDatabase>
#include <QPixmap>
#include <QProcess>
#include <QSplitter>
#include <QStyle>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QScrollBar>
#include <QUrl>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QBrush>
#include <QColor>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <algorithm>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <QMetaObject>

namespace dirtoo::app {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
  setWindowTitle(QStringLiteral("dirtoo"));
  resize(960, 640);

  setup_background_workers();
  setup_toolbar();
  setup_menus();
  setup_shortcuts();
  setup_central_ui();
  setup_status_and_services();

  update_history_actions();
  update_edit_actions();
  set_view_mode(ViewMode::Detail);
  restore_settings();
}

MainWindow::~MainWindow()
{
  stop_search();
  if (path_completion_worker_ != nullptr) {
    path_completion_worker_->cancel();
  }
  if (path_completion_thread_ != nullptr) {
    path_completion_thread_->quit();
    path_completion_thread_->wait(2000);
  }
  transfer_controller_.shutdown();
  search_controller_.stop();
  if (dir_load_thread_ != nullptr) {
    dir_load_thread_->quit();
    dir_load_thread_->wait(3000);
  }
  if (sort_thread_ != nullptr) {
    sort_thread_->quit();
    sort_thread_->wait(3000);
  }
  if (filter_thread_ != nullptr) {
    filter_thread_->quit();
    filter_thread_->wait(3000);
  }
  if (dir_thumb_thread_ != nullptr) {
    dir_thumb_thread_->quit();
    dir_thumb_thread_->wait(3000);
  }
}

QAbstractItemView* MainWindow::current_view() const
{
  if (view_mode_ == ViewMode::Detail) {
    return tree_view_;
  }
  return icon_view_;
}


void MainWindow::set_status(const QString& text)
{
  if (status_label_ != nullptr) {
    status_label_->setText(text);
  }
  if (!text.isEmpty()) {
    qInfo().noquote() << QStringLiteral("status: %1").arg(text);
  }
}

void MainWindow::update_edit_actions()
{
  update_mutation_actions();
}


void MainWindow::showEvent(QShowEvent* event)
{
  QMainWindow::showEvent(event);
  // Tool buttons exist after the toolbar is realized.
  if (parent_act_ != nullptr) {
    for (auto* tb : findChildren<QToolButton*>()) {
      if (tb->defaultAction() == parent_act_) {
        tb->installEventFilter(this);
      }
    }
  }
  // Prefer the file view for keyboard navigation right after startup
  // (type-ahead, Home/End, arrows) instead of the location bar or toolbar.
  if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
    graphics_view_->setFocus(Qt::OtherFocusReason);
  } else if (QAbstractItemView* view = current_view()) {
    view->setFocus(Qt::OtherFocusReason);
  }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
  persist_settings();
  QMainWindow::closeEvent(event);
}


void MainWindow::on_refresh()
{
  on_directory_changed();
  set_status(QStringLiteral("Refreshed"));
}


} // namespace dirtoo::app

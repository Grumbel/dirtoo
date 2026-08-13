// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"
#include "activity_monitor.hpp"

#include "badge_icons.hpp"
#include "location_icons.hpp"
#include "location_url.hpp"
#include "location_menu_helpers.hpp"
#include "udisks_client.hpp"
#include "devices_controller.hpp"
#include "file_views.hpp"
#include "file_context_menu.hpp"
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
#include <QActionGroup>
#include <QtConcurrent>
#include "dirtoo/archive/archive_index.hpp"
#include <QDateTime>
#include <QCoreApplication>
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
#include <QKeySequence>
#include <QLocale>
#include <QMenuBar>
#include <QPushButton>
#include <QTextBrowser>
#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMimeDatabase>
#include <QPixmap>
#include <QProcess>
#include <QStyle>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
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
#include <filesystem>
#include <optional>

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

  // B8: keep extract caches from growing forever (open / thumbs / drop + legacy /tmp).
  {
    const auto pruned = prune_all_archive_member_caches();
    if (pruned.trees_removed > 0 || pruned.bytes_removed > 0) {
      qInfo().noquote() << QStringLiteral(
          "archive cache prune: removed %1 tree(s), %2 bytes")
                             .arg(pruned.trees_removed)
                             .arg(pruned.bytes_removed);
    }
  }
}

MainWindow::~MainWindow()
{
  stop_search();
  location_chrome_.shutdown();
  transfer_controller_.shutdown();
  search_controller_.stop();
  list_workers_.shutdown();
  shutdown_thumbnail_workers();
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
  update_busy_indicator(text);
}

void MainWindow::update_busy_indicator(const QString& activity)
{
  // Heuristic: activity strings that end with an ellipsis or known verbs indicate
  // background work. Also respect an active transfer.
  const bool transfer_busy = transfer_controller_.busy();
  const QString t = activity.trimmed();
  const bool text_busy = t.endsWith(QChar(0x2026)) || t.endsWith(QStringLiteral("..."))
                         || t.startsWith(QStringLiteral("Loading"))
                         || t.startsWith(QStringLiteral("Refreshing"))
                         || t.startsWith(QStringLiteral("Filtering"))
                         || t.startsWith(QStringLiteral("Searching"))
                         || t.startsWith(QStringLiteral("Sorting"))
                         || t.startsWith(QStringLiteral("Transfer"))
                         || t.startsWith(QStringLiteral("Tagging"))
                         || t.startsWith(QStringLiteral("Regenerating"))
                         || t.startsWith(QStringLiteral("Preparing"))
                         || t.startsWith(QStringLiteral("Building"));

  auto& mon = ActivityMonitor::instance();
  if (transfer_busy) {
    mon.set_task(QStringLiteral("transfer"), QStringLiteral("Transfer"));
  } else {
    mon.clear_task(QStringLiteral("transfer"));
  }
  if (text_busy && !t.isEmpty()) {
    // Collapse generic status into one task so the toolbar stays readable.
    mon.set_task(QStringLiteral("status"), t);
  } else {
    mon.clear_task(QStringLiteral("status"));
  }

  if (busy_label_ == nullptr) {
    return;
  }
  const bool busy = transfer_busy || text_busy || mon.any_active();
  busy_label_->setVisible(busy);
  if (busy) {
    QString tip = mon.headline();
    if (tip.isEmpty() || tip == QLatin1String("Idle")) {
      tip = t.isEmpty() ? QStringLiteral("Working…") : t;
    }
    busy_label_->setToolTip(tip);
  } else {
    busy_label_->setToolTip(QStringLiteral("Idle"));
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

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

void MainWindow::setup_background_workers()
{
  connect(&transfer_controller_, &TransferController::item_started, this,
          &MainWindow::on_transfer_item_started);
  connect(&transfer_controller_, &TransferController::byte_progress, this,
          &MainWindow::on_transfer_byte_progress);
  connect(&transfer_controller_, &TransferController::conflict_required, this,
          &MainWindow::on_transfer_conflict);
  connect(&transfer_controller_, &TransferController::finished, this,
          &MainWindow::on_transfer_finished);
  connect(&transfer_controller_, &TransferController::log_line, this, [this](const QString& line) {
    qInfo().noquote() << QStringLiteral("transfer: %1").arg(line);
    if (transfer_controller_.dialog() != nullptr) {
      transfer_controller_.dialog()->append_log(line);
    }
  });

  connect(&search_controller_, &SearchController::match_found, this, &MainWindow::on_search_match);
  connect(&search_controller_, &SearchController::progress, this, &MainWindow::on_search_progress);
  connect(&search_controller_, &SearchController::finished, this, &MainWindow::on_search_finished);

  dir_load_worker_ = new DirectoryLoadWorker;
  dir_load_thread_ = new QThread(this);
  dir_load_worker_->moveToThread(dir_load_thread_);
  connect(dir_load_thread_, &QThread::finished, dir_load_worker_, &QObject::deleteLater);
  connect(dir_load_worker_, &DirectoryLoadWorker::loaded, this, &MainWindow::on_directory_loaded);
  connect(dir_load_worker_, &DirectoryLoadWorker::failed, this, &MainWindow::on_directory_load_failed);
  qRegisterMetaType<std::vector<dirtoo::fs::FileInfo>>("std::vector<dirtoo::fs::FileInfo>");
  dir_load_thread_->start();

  sort_worker_ = new SortWorker;
  sort_thread_ = new QThread(this);
  sort_worker_->moveToThread(sort_thread_);
  connect(sort_thread_, &QThread::finished, sort_worker_, &QObject::deleteLater);
  connect(sort_worker_, &SortWorker::sorted, this, &MainWindow::on_sort_finished);
  qRegisterMetaType<dirtoo::collection::SortKey>("dirtoo::collection::SortKey");
  sort_thread_->start();

  filter_worker_ = new FilterWorker;
  filter_thread_ = new QThread(this);
  filter_worker_->moveToThread(filter_thread_);
  connect(filter_thread_, &QThread::finished, filter_worker_, &QObject::deleteLater);
  connect(filter_worker_, &FilterWorker::filtered, this, &MainWindow::on_filter_finished);
  qRegisterMetaType<dirtoo::collection::GroupMode>("dirtoo::collection::GroupMode");
  filter_thread_->start();

  dir_thumb_worker_ = new DirectoryThumbnailWorker;
  dir_thumb_thread_ = new QThread(this);
  dir_thumb_worker_->moveToThread(dir_thumb_thread_);
  connect(dir_thumb_thread_, &QThread::finished, dir_thumb_worker_, &QObject::deleteLater);
  connect(dir_thumb_worker_, &DirectoryThumbnailWorker::thumbnail_ready, this,
          &MainWindow::on_thumbnail_ready);
  connect(dir_thumb_worker_, &DirectoryThumbnailWorker::finished, this,
          [this](int ok, int fail) {
            set_status(QStringLiteral("Directory thumbnails: %1 ok, %2 failed").arg(ok).arg(fail));
          });
  dir_thumb_thread_->start();
}


void MainWindow::setup_shortcuts()
{
  // Only shortcuts not already bound on menu/toolbar actions (those are
  // ambiguous if registered again via addAction).
  auto add_shortcut = [this](const QKeySequence& seq, auto slot) {
    auto* act = new QAction(this);
    act->setShortcut(seq);
    connect(act, &QAction::triggered, this, slot);
    addAction(act);
    return act;
  };
  add_shortcut(QKeySequence(Qt::Key_Backspace), &MainWindow::on_go_parent);
  add_shortcut(QKeySequence(Qt::ALT | Qt::Key_Up), &MainWindow::on_go_parent);
  add_shortcut(QKeySequence(Qt::ALT | Qt::Key_Home), &MainWindow::on_go_home);
  add_shortcut(QKeySequence(Qt::ALT | Qt::Key_Left), &MainWindow::on_go_back);
  add_shortcut(QKeySequence(Qt::ALT | Qt::Key_Right), &MainWindow::on_go_forward);
  add_shortcut(QKeySequence(QStringLiteral("Ctrl+L")), &MainWindow::on_focus_location);
  add_shortcut(QKeySequence(Qt::Key_Escape), &MainWindow::on_clear_filter);
  add_shortcut(QKeySequence(QStringLiteral("Ctrl+D")), &MainWindow::on_toggle_bookmark);
}

void MainWindow::setup_status_and_services()
{
  // Real QStatusBar: left = filename / messages, right = size info (dirtoo-py).
  status_label_ = new QLabel(this);
  statusBar()->addWidget(status_label_, 1);
  status_info_label_ = new QLabel(this);
  status_info_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  statusBar()->addPermanentWidget(status_info_label_);
  statusBar()->setSizeGripEnabled(true);

  leap_widget_ = new LeapWidget(this);
  connect(leap_widget_, &LeapWidget::leap, this, &MainWindow::on_leap);
  connect(leap_widget_, &LeapWidget::closed, this, [this] {
    if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
      graphics_view_->setFocus(Qt::OtherFocusReason);
    } else if (QAbstractItemView* view = current_view()) {
      view->setFocus(Qt::OtherFocusReason);
    }
  });

  // Debounce watcher storms (busy dirs otherwise clear+reload+rethumb every event).
  watcher_reload_timer_ = new QTimer(this);
  watcher_reload_timer_->setSingleShot(true);
  watcher_reload_timer_->setInterval(200);
  connect(watcher_reload_timer_, &QTimer::timeout, this, [this] { reload_directory(true); });
  connect(&watcher_, &watcher::DirectoryWatcher::directory_changed, this, [this] {
  if (watcher_reload_timer_ != nullptr) {
    watcher_reload_timer_->start();
  }
  });
  connect(&watcher_, &watcher::DirectoryWatcher::entries_changed, this,
          &MainWindow::on_entries_changed);
  connect(&watcher_, &watcher::DirectoryWatcher::message, this, [this](const QString& msg) {
  set_status(msg);
  });

  connect(&thumbnailer_, &thumbnail::Thumbnailer::thumbnail_ready, this,
        &MainWindow::on_thumbnail_ready);
  connect(&thumbnailer_, &thumbnail::Thumbnailer::thumbnail_failed, this,
        &MainWindow::on_thumbnail_failed);

  connect(qApp->clipboard(), &QClipboard::dataChanged, this, &MainWindow::update_edit_actions);

  connect(&archive_manager_, &archive::ArchiveManager::extraction_started, this,
        [this](const fs::Location&) {
          set_status(QStringLiteral("Extracting archive…"));
        });
  connect(&archive_manager_, &archive::ArchiveManager::extraction_ready, this,
        &MainWindow::on_archive_ready);
  connect(&archive_manager_, &archive::ArchiveManager::extraction_failed, this,
        &MainWindow::on_archive_failed);

}


} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"
#include "tag_paint.hpp"

#include <QDebug>

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

  tag_.set_dialog_parent(this);
  connect(&file_sets_, &FileSetController::status_message, this,
          [this](const QString& msg, int ms) { set_status(msg, ms); });
  file_sets_.set_dialog_parent(this);
  connect(&tag_, &TagController::status_message, this,
          [this](const QString& text, int timeout_ms) {
            if (statusBar() != nullptr) {
              statusBar()->showMessage(text, timeout_ms);
            }
            // Surface skip/failure batches in the non-modal banner; Activity log has full detail.
            if (message_area_ != nullptr
                && (text.contains(QStringLiteral("failed"), Qt::CaseInsensitive)
                    || text.contains(QStringLiteral("Skipped")))) {
              message_area_->show_error(text, timeout_ms);
            }
          });
  connect(&tag_, &TagController::tags_applied, this, [this](int) {
    tag_paint_detail::clear_tag_chip_cache();
    filter_search_.refresh_filter_completions();
    if (graphics_view_ != nullptr) {
      graphics_view_->viewport()->update();
    }
    if (icon_view_ != nullptr) {
      icon_view_->viewport()->update();
    }
    if (tree_view_ != nullptr) {
      tree_view_->viewport()->update();
    }
  });

  list_workers_.setup();
  connect(list_workers_.dir_load(), &DirectoryLoadWorker::loaded, this, &MainWindow::on_directory_loaded);
  connect(list_workers_.dir_load(), &DirectoryLoadWorker::failed, this, &MainWindow::on_directory_load_failed);
  connect(list_workers_.dir_load(), &DirectoryLoadWorker::progress, this,
          &MainWindow::on_directory_load_progress);
  connect(list_workers_.sort(), &SortWorker::sorted, this, &MainWindow::on_sort_finished);
  connect(list_workers_.filter(), &FilterWorker::filtered, this, &MainWindow::on_filter_finished);

  thumbs_.setup_dir_worker();
  if (thumbs_.dir_worker() != nullptr) {
    connect(thumbs_.dir_worker(), &DirectoryThumbnailWorker::finished, this,
            [this](int ok, int fail) {
              if (fail > 0) {
                // Per-path reasons already went to stderr from the worker;
                // keep the status bar summary and a single roll-up warning.
                qWarning().noquote()
                    << QStringLiteral("dirtoo: directory thumbnails finished: %1 ok, %2 failed "
                                      "(see earlier stderr lines for reasons)")
                           .arg(ok)
                           .arg(fail);
                set_status(QStringLiteral("Directory thumbnails: %1 ok, %2 failed (see stderr)")
                               .arg(ok)
                               .arg(fail));
              } else {
                set_status(QStringLiteral("Directory thumbnails: %1 ok, %2 failed").arg(ok).arg(fail));
              }
            });
  }
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
  add_shortcut(QKeySequence(QStringLiteral("Ctrl+K")), &MainWindow::on_focus_filter);
  add_shortcut(QKeySequence(Qt::Key_Escape), &MainWindow::on_clear_filter);
  add_shortcut(QKeySequence(QStringLiteral("Ctrl+D")), &MainWindow::on_toggle_bookmark);
  // Persistent ad-hoc file sets (not View → Group By).
  add_shortcut(QKeySequence(QStringLiteral("Ctrl+G")), &MainWindow::create_set_from_selection);
  add_shortcut(QKeySequence(QStringLiteral("Ctrl+Shift+G")), &MainWindow::add_selection_to_last_set);
}

void MainWindow::setup_status_and_services()
{
  // Real QStatusBar: left = filename / messages, busy badge, right = size info.
  status_label_ = new QLabel(this);
  statusBar()->addWidget(status_label_, 1);
  busy_label_ = new QLabel(this);
  busy_label_->setFixedSize(18, 18);
  busy_label_->setScaledContents(true);
  busy_label_->setToolTip(QStringLiteral("Idle"));
  busy_label_->hide();
  {
    const QPixmap pm = load_badge_pixmap(QStringLiteral("badge-loading.png"));
    if (!pm.isNull()) {
      busy_label_->setPixmap(pm);
    } else {
      busy_label_->setText(QStringLiteral("…"));
    }
  }
  statusBar()->addPermanentWidget(busy_label_);
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

  wire_thumbnail_services();

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

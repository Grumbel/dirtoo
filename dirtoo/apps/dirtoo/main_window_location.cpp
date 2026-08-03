// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"
#include <QMenu>

#include "theme_icons.hpp"
#include "location_menu_helpers.hpp"
#include "open_history.hpp"
#include "message_area.hpp"
#include "location_button_bar.hpp"
#include "path_completion_worker.hpp"
#include "leap_widget.hpp"
#include "history_menu.hpp"

#include <QCompleter>
#include <QLineEdit>
#include <QMetaObject>
#include <QStringListModel>
#include <QTimer>
#include <QThread>

namespace dirtoo::app {

void MainWindow::on_focus_location()
{
  show_location_line_edit();
  location_edit_->setFocus(Qt::ShortcutFocusReason);
  location_edit_->selectAll();
}

MainWindow* MainWindow::open_new_window(const fs::Location& location)
{
  auto* win = new MainWindow;
  win->setAttribute(Qt::WA_DeleteOnClose);
  win->open_location(location);
  win->show();
  win->raise();
  win->activateWindow();
  return win;
}

void MainWindow::on_new_window()
{
  open_new_window(location_);
}

void MainWindow::on_breadcrumb_location_new_window(const fs::Location& location)
{
  open_new_window(location);
}

void MainWindow::on_breadcrumb_location(const fs::Location& location)
{
  open_location(location);
}

void MainWindow::on_location_edit_requested()
{
  show_location_line_edit();
  location_edit_->setFocus(Qt::MouseFocusReason);
  location_edit_->selectAll();
}

void MainWindow::show_location_buttons()
{
  if (location_buttons_ != nullptr) {
    location_buttons_->show();
  }
  if (location_edit_ != nullptr) {
    location_edit_->hide();
  }
}

void MainWindow::show_location_line_edit()
{
  if (location_buttons_ != nullptr) {
    location_buttons_->hide();
  }
  if (location_edit_ != nullptr) {
    location_edit_->show();
  }
}

void MainWindow::on_location_text_edited(const QString& text)
{
  path_completion_pending_ = text;
  if (path_completion_timer_ != nullptr) {
    path_completion_timer_->start();
  }
}

void MainWindow::on_path_completion_timeout()
{
  if (path_completion_worker_ == nullptr || path_completion_thread_ == nullptr) {
    return;
  }
  const QString text = path_completion_pending_;
  if (text.isEmpty()) {
    if (path_completion_model_ != nullptr) {
      path_completion_model_->setStringList({});
    }
    return;
  }
  const quint64 id = ++path_completion_request_id_;
  QMetaObject::invokeMethod(
      path_completion_worker_,
      [worker = path_completion_worker_, id, text] { worker->complete(id, text); },
      Qt::QueuedConnection);
}

void MainWindow::on_path_completions_ready(quint64 request_id, const QString& longest,
                                           const QStringList& candidates)
{
  (void)longest;
  if (request_id != path_completion_request_id_) {
    return; // stale
  }
  if (path_completion_model_ == nullptr) {
    return;
  }
  path_completion_model_->setStringList(candidates);
  if (path_completer_ != nullptr && location_edit_ != nullptr && location_edit_->hasFocus()
      && !candidates.isEmpty()) {
    path_completer_->complete();
  }
}

void MainWindow::on_parent_new_window()
{
  open_new_window(location_.parent());
}

void MainWindow::on_rebuild_history_menu()
{
  if (history_menu_ == nullptr) {
    return;
  }
  history_menu_->clear();
  history_menu_->addAction(theme_icon("go-previous", "arrow-left"), QStringLiteral("Back"), this,
                           &MainWindow::on_go_back);
  history_menu_->addAction(theme_icon("go-next", "arrow-right"), QStringLiteral("Forward"), this,
                           &MainWindow::on_go_forward);
  history_menu_->addSeparator();

  // Folder / location history — most recent first.
  std::vector<fs::Location> locs;
  int count = 0;
  for (auto it = nav_history_.unique_locations().rbegin();
       it != nav_history_.unique_locations().rend() && count < 35; ++it, ++count) {
    locs.push_back(*it);
  }
  add_location_menu_entries(
      history_menu_, locs, this,
      [this](const fs::Location& loc) { open_location(loc); },
      [this](const fs::Location& loc) { open_new_window(loc); });
}

void MainWindow::on_rebuild_recent_opens_menu()
{
  if (recent_opens_menu_ == nullptr) {
    return;
  }
  populate_recent_opens_menu(recent_opens_menu_, 30);
  recent_opens_menu_->addSeparator();
  recent_opens_menu_->addAction(theme_icon("view-history", "document-open-recent"),
                                QStringLiteral("Open History…"), this, [this] {
                                  show_open_history_dialog(this, [this](const QString& dir) {
                                    open_location(fs::Location::from_path(dir.toStdString()));
                                  });
                                });
}

void MainWindow::on_toggle_bookmark()
{
  if (location_.empty()) {
    return;
  }
  const bool now = bookmarks_.toggle(location_);
  if (message_area_ != nullptr) {
    if (now) {
      message_area_->show_info(QStringLiteral("Bookmark added"));
    } else {
      message_area_->show_info(QStringLiteral("Bookmark removed"));
    }
  }
  set_status(now ? QStringLiteral("Bookmarked") : QStringLiteral("Bookmark removed"));
  rebuild_sidebar_places();
}

void MainWindow::on_rebuild_bookmarks_menu()
{
  if (bookmarks_menu_ == nullptr) {
    return;
  }
  bookmarks_menu_->clear();

  if (location_.empty()) {
    auto* act = bookmarks_menu_->addAction(QStringLiteral("No location to bookmark"));
    act->setEnabled(false);
  } else if (bookmarks_.contains(location_)) {
    bookmarks_menu_->addAction(theme_icon("bookmark-remove", "list-remove"),
                               QStringLiteral("Remove Bookmark for This Location"), this,
                               &MainWindow::on_toggle_bookmark);
  } else {
    bookmarks_menu_->addAction(theme_icon("bookmark-new", "list-add"),
                               QStringLiteral("Bookmark This Location"), this,
                               &MainWindow::on_toggle_bookmark);
  }
  bookmarks_menu_->addSeparator();

  const auto entries = bookmarks_.entries();
  if (entries.empty()) {
    auto* act = bookmarks_menu_->addAction(QStringLiteral("(no bookmarks yet)"));
    act->setEnabled(false);
    return;
  }

  add_location_menu_entries(
      bookmarks_menu_, entries, this,
      [this](const fs::Location& loc) { open_location(loc); },
      [this](const fs::Location& loc) { open_new_window(loc); });
}

} // namespace dirtoo::app

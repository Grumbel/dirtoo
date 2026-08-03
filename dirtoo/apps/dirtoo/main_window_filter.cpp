// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include "dirtoo/filter/parser.hpp"
#include <QDialog>
#include <QPalette>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QColor>

namespace dirtoo::app {

void MainWindow::on_show_filter_help()
{
  auto* dlg = new QDialog(this);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setWindowTitle(QStringLiteral("Filter expression help"));
  dlg->resize(560, 720);
  auto* v = new QVBoxLayout(dlg);
  auto* browser = new QTextBrowser(dlg);
  browser->setOpenExternalLinks(false);
  browser->setHtml(QString::fromStdString(dirtoo::filter::filter_help_html()));
  v->addWidget(browser, 1);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
  connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
  // Close button maps to rejected for Close-only box
  connect(buttons, &QDialogButtonBox::clicked, dlg, [dlg](QAbstractButton*) { dlg->close(); });
  v->addWidget(buttons);
  dlg->show();
}

void MainWindow::update_filter_chrome(bool filtered)
{
  // Tint only the file views when a filter is active (dirtoo-py set_filtered).
  // Keep the filter bar on the window/chrome palette so it does not pick up the
  // view Base color (which made the bar look “merged” into the list).
  const QColor tint(220, 220, 255);
  const QColor app_base = qApp->palette().color(QPalette::Base);
  const QColor app_window = qApp->palette().color(QPalette::Window);

  auto apply_view_bg = [&](QWidget* w) {
    if (w == nullptr) {
      return;
    }
    QPalette pal = w->palette();
    pal.setColor(QPalette::Base, filtered ? tint : app_base);
    w->setPalette(pal);
  };
  apply_view_bg(tree_view_);
  apply_view_bg(icon_view_);
  if (graphics_view_ != nullptr) {
    if (filtered) {
      graphics_view_->setBackgroundBrush(QBrush(tint));
    } else {
      graphics_view_->setBackgroundBrush(QBrush(app_base));
    }
  }

  if (filter_row_ != nullptr) {
    // Explicit chrome background; never inherit the tinted view Base.
    filter_row_->setAutoFillBackground(true);
    QPalette bar_pal = filter_row_->palette();
    bar_pal.setColor(QPalette::Window, app_window);
    bar_pal.setColor(QPalette::Base, app_base);
    filter_row_->setPalette(bar_pal);
    filter_row_->setStyleSheet(QString());
  }
  if (filter_edit_ != nullptr) {
    QPalette edit_pal = filter_edit_->palette();
    edit_pal.setColor(QPalette::Base, app_base);
    edit_pal.setColor(QPalette::Window, app_base);
    filter_edit_->setPalette(edit_pal);
  }
}

void MainWindow::on_filter_changed(const QString& text)
{
  update_filter_chrome(!text.isEmpty());
  if (filter_expression_needs_content_io(text)) {
    // Content predicates must not run on the GUI thread.
    request_async_filter();
    return;
  }
  collection_.set_name_filter(text.toStdString());
  refresh_list();
  request_thumbnails_for_visible();
  if (!text.isEmpty() && !collection_.filter_parse_ok()) {
    if (message_area_ != nullptr) {
      message_area_->show_info(QStringLiteral("Filter parse issue — using substring fallback"));
    }
  }
}

void MainWindow::request_async_filter(bool keep_previous_visible)
{
  if (filter_worker_ == nullptr) {
    collection_.set_name_filter(
        filter_edit_ != nullptr ? filter_edit_->text().toStdString() : std::string{});
    refresh_list();
    return;
  }
  const quint64 gen = ++filter_generation_;
  set_status(QStringLiteral("Filtering…"));
  auto items = collection_.items();
  const QString expr = filter_edit_ != nullptr ? filter_edit_->text() : QString();
  const bool show_hidden = collection_.show_hidden();
  const auto group_mode = collection_.group_mode();
  // Avoid GUI-thread content I/O. For interactive filter changes, clear visible
  // until the worker finishes; for soft watcher reloads keep the previous list.
  if (!keep_previous_visible) {
    collection_.replace_visible({}, true);
    refresh_list();
  }
  QMetaObject::invokeMethod(
      filter_worker_, "filter_items", Qt::QueuedConnection,
      Q_ARG(std::vector<dirtoo::fs::FileInfo>, items), Q_ARG(QString, expr),
      Q_ARG(bool, show_hidden), Q_ARG(dirtoo::collection::GroupMode, group_mode),
      Q_ARG(quint64, gen));
}

void MainWindow::on_filter_finished(quint64 generation, std::vector<dirtoo::fs::FileInfo> visible,
                                    bool parse_ok)
{
  if (generation != filter_generation_ || search_active_) {
    return;
  }
  // Apply current sort to the filtered list (in-memory only; no FS I/O).
  collection_.sorter().sort(visible);
  collection_.replace_visible(std::move(visible), parse_ok);
  refresh_list();
  request_thumbnails_for_visible();
  if (!parse_ok && message_area_ != nullptr) {
    message_area_->show_info(QStringLiteral("Filter parse issue — using substring fallback"));
  }
  set_status(QStringLiteral("%1 items").arg(collection_.visible_items().size()));
}

void MainWindow::on_toggle_filter_visible()
{
  if (show_filter_act_ == nullptr) {
    return;
  }
  show_filter_act_->toggle();
}


void MainWindow::on_show_search()
{
  stop_search();
  if (search_edit_ == nullptr) {
    return;
  }
  if (search_row_ != nullptr) {
    search_row_->setVisible(true);
  } else if (search_edit_ != nullptr) {
    search_edit_->setVisible(true);
  }
  search_edit_->setFocus(Qt::ShortcutFocusReason);
  search_edit_->selectAll();
}

void MainWindow::stop_search()
{
  search_controller_.stop();
  search_batch_.clear();
}


void MainWindow::on_search_submitted()
{
  if (search_edit_ == nullptr) {
    return;
  }
  const QString expr = search_edit_->text().trimmed();
  if (expr.isEmpty()) {
    return;
  }
  if (location_.is_archive()) {
    set_status(QStringLiteral("Recursive search is not available inside archives"));
    return;
  }

  stop_search();
  search_results_.clear();
  search_batch_.clear();
  search_status_matched_ = 0;
  search_active_ = true;
  collection_.clear();
  collection_.clear_filter();
  refresh_list();

  const QString root = QString::fromStdString(location_.as_path().string());
  const bool show_hidden = show_hidden_act_ != nullptr && show_hidden_act_->isChecked();
  set_status(QStringLiteral("Searching…"));
  if (message_area_ != nullptr) {
    message_area_->show_info(QStringLiteral("Recursive search: %1").arg(expr));
  }
  search_controller_.start(root, expr, show_hidden, /*max_depth=*/-1);
}

void MainWindow::on_search_match(const QString& path, bool is_directory, quint64 size)
{
  if (!search_active_) {
    return;
  }
  // Synthetic only: avoid FileInfo::from_path (stat) on the GUI thread for every hit.
  const std::filesystem::path p{path.toStdString()};
  auto info = fs::FileInfo::synthetic(fs::Location::from_path(p), p.filename().string(),
                                      is_directory, size);
  search_results_.push_back(info);
  search_batch_.push_back(std::move(info));
  // Incremental inserts (batched) beat periodic full layoutChanged on large sets.
  // Early results flush soon for responsiveness; larger batches later reduce model churn.
  if (search_batch_.size() >= 48 || search_results_.size() < 24) {
    flush_search_batch();
  } else if (search_batch_.size() == 1) {
    // Schedule a deferred flush so a trickle of hits still paints without waiting
    // for a full batch (or search end).
    QTimer::singleShot(50, this, [this] {
      if (search_active_) {
        flush_search_batch();
      }
    });
  }
}

void MainWindow::flush_search_batch()
{
  if (search_batch_.empty()) {
    return;
  }
  const int count = static_cast<int>(search_batch_.size());
  collection_.append_visible_items(std::move(search_batch_));
  search_batch_.clear();
  if (model_ != nullptr) {
    model_->notify_rows_appended(count);
  } else {
    refresh_list();
  }
  update_status_selection();
}

void MainWindow::on_search_progress(quint64 visited, quint64 matched)
{
  (void)visited;
  // Throttle status updates — progress can fire very frequently on huge trees.
  if (matched == search_status_matched_) {
    return;
  }
  if (matched < 32 || matched - search_status_matched_ >= 32) {
    search_status_matched_ = matched;
    set_status(QStringLiteral("Searching… %1 matches").arg(matched));
  }
}

void MainWindow::on_search_finished(quint64 matched, quint64 visited, const QString& error)
{
  flush_search_batch();
  request_thumbnails_for_visible();
  if (!error.isEmpty() && error != QStringLiteral("cancelled")) {
    set_status(error);
    if (message_area_ != nullptr) {
      message_area_->show_info(error);
    }
  } else if (error == QStringLiteral("cancelled")) {
    set_status(
        QStringLiteral("Search cancelled — %1 matches (%2 visited)").arg(matched).arg(visited));
  } else {
    set_status(
        QStringLiteral("Search done — %1 matches (%2 visited)").arg(matched).arg(visited));
  }
  // Thread lifecycle owned by SearchController.
  // Keep search_active_ true so directory watcher does not wipe results until user navigates.
}

void MainWindow::on_clear_filter()
{
  if (search_row_ != nullptr ? search_row_->isVisible()
      : (search_edit_ != nullptr && search_edit_->isVisible())) {
    stop_search();
    search_active_ = false;
    search_results_.clear();
    if (search_row_ != nullptr) {
      search_row_->hide();
    } else if (search_edit_ != nullptr) {
      search_edit_->hide();
    }
    search_edit_->clear();
    on_directory_changed();
    return;
  }
  if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()) {
    filter_edit_->clear();
    return;
  }
  if ((filter_row_ != nullptr ? filter_row_->isVisible()
         : (filter_edit_ != nullptr && filter_edit_->isVisible()))
      && !filter_pinned_
      && show_filter_act_ != nullptr && show_filter_act_->isChecked()) {
    show_filter_act_->setChecked(false);
    return;
  }
  if (location_edit_ != nullptr && location_edit_->isVisible()) {
    show_location_buttons();
  }
}


} // namespace dirtoo::app

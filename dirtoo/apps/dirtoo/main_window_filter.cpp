// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"
#include "set_membership.hpp"
#include <vector>
#include "dirtoo/sets/file_set_store.hpp"

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

  // Explicit chrome background; never inherit the tinted view Base.
  filter_search_.reset_filter_bar_palette();
}

void MainWindow::apply_filter_expression_sync(const QString& text)
{
  const std::string expr = text.toStdString();
  std::vector<std::string> set_queries;
  if (auto q = set_membership::pure_set_query(expr)) {
    set_queries.push_back(*q);
  } else if (auto qs = set_membership::pure_set_or_queries(expr)) {
    set_queries = *qs;
  }
  if (!set_queries.empty()) {
    dirtoo::sets::FileSetStore store;
    std::string err;
    std::vector<set_membership::MemberIndex> indexes;
    if (store.open(dirtoo::sets::FileSetStore::default_path(), &err)) {
      for (const auto& q : set_queries) {
        if (auto id = set_membership::resolve_set_id(store, q)) {
          indexes.push_back(set_membership::build_member_index(store, *id));
        }
      }
    }
    std::vector<fs::FileInfo> visible;
    visible.reserve(collection_.items().size());
    for (const auto& fi : collection_.items()) {
      if (!collection_.show_hidden() && !fi.basename().empty() && fi.basename()[0] == '.') {
        continue;
      }
      bool hit = false;
      for (const auto& idx : indexes) {
        if (set_membership::path_in_index(idx, fi.path())) {
          hit = true;
          break;
        }
      }
      if (hit) {
        visible.push_back(fi);
      }
    }
    collection_.sorter().sort(visible);
    collection_.replace_visible(std::move(visible), true);
    refresh_list();
    request_thumbnails_for_visible();
    set_status(QStringLiteral("%1 items").arg(collection_.visible_items().size()));
    return;
  }
  collection_.set_name_filter(text.toStdString());
  refresh_list();
  request_thumbnails_for_visible();
  set_status(QStringLiteral("%1 items").arg(collection_.visible_items().size()));
}

void MainWindow::on_filter_changed(const QString& text)
{
  if (quick_filter_bar_ != nullptr) {
    quick_filter_bar_->set_active_expression(text);
  }
  update_filter_chrome(!text.isEmpty());
  // Pure set:… or set:a OR set:b → sync membership index (fast); content IO → worker.
  {
    const std::string expr = text.toStdString();
    if (set_membership::pure_set_query(expr).has_value()
        || set_membership::pure_set_or_queries(expr).has_value()) {
      apply_filter_expression_sync(text);
      return;
    }
  }
  if (filter_expression_needs_content_io(text)) {
    request_async_filter(/*keep_previous_visible=*/true);
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
  // Directory still loading: filter is stored and will re-apply in on_directory_loaded.
  if (!text.isEmpty() && collection_.items().empty()) {
    set_status(QStringLiteral("Filter ready — waiting for directory listing…"));
  } else if (!text.isEmpty()) {
    set_status(QStringLiteral("%1 items").arg(collection_.visible_items().size()));
  }
}

void MainWindow::request_async_filter(bool keep_previous_visible)
{
  if (list_workers_.filter() == nullptr) {
    // No worker thread: apply set: on the GUI via FileSetStore, else name filter.
    apply_filter_expression_sync(filter_search_.filter_text());
    return;
  }
  const quint64 gen = list_workers_.next_filter_generation();
  auto items = collection_.items();
  const QString expr = filter_search_.filter_text();
  const bool show_hidden = collection_.show_hidden();
  const auto group_mode = collection_.group_mode();
  // Always keep the previous visible list while the worker runs so the UI does
  // not stall empty (progressive: show current data, refine when ready).
  // keep_previous_visible is retained for callers that still want an explicit
  // clear (none currently do for interactive typing).
  if (!keep_previous_visible) {
    collection_.replace_visible({}, true);
    refresh_list();
  }
  const int still_showing = static_cast<int>(collection_.visible_items().size());
  if (still_showing > 0) {
    set_status(QStringLiteral("Filtering… (%1 still shown)").arg(still_showing));
  } else {
    set_status(QStringLiteral("Filtering…"));
  }
  QMetaObject::invokeMethod(
      list_workers_.filter(), "filter_items", Qt::QueuedConnection,
      Q_ARG(std::vector<dirtoo::fs::FileInfo>, items), Q_ARG(QString, expr),
      Q_ARG(bool, show_hidden), Q_ARG(dirtoo::collection::GroupMode, group_mode),
      Q_ARG(quint64, gen));
}

void MainWindow::on_filter_finished(quint64 generation, std::vector<dirtoo::fs::FileInfo> visible,
                                    bool parse_ok)
{
  if (generation != list_workers_.filter_generation() || search_session_.active) {
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

void MainWindow::on_focus_filter()
{
  // Ctrl+K always shows the filter bar and focuses the edit (does not toggle).
  if (show_filter_act_ != nullptr && !show_filter_act_->isChecked()) {
    show_filter_act_->setChecked(true);
  } else {
    filter_search_.set_filter_visible(true);
  }
  filter_search_.focus_filter(Qt::ShortcutFocusReason);
}


void MainWindow::on_show_search()
{
  stop_search();
  if (filter_search_.search_edit() == nullptr) {
    return;
  }
  filter_search_.set_search_visible(true);
  filter_search_.focus_search(Qt::ShortcutFocusReason);
}

void MainWindow::stop_search()
{
  search_controller_.stop();
  search_session_.batch.clear();
}


void MainWindow::on_search_submitted()
{
  const QString expr = filter_search_.search_text().trimmed();
  if (expr.isEmpty()) {
    return;
  }
  if (location_.is_archive()) {
    set_status(QStringLiteral("Recursive search is not available inside archives"));
    return;
  }

  stop_search();
  search_session_.results.clear();
  search_session_.batch.clear();
  search_session_.status_matched = 0;
  search_session_.active = true;
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

void MainWindow::on_search_match(const QString& path, bool is_directory, quint64 size,
                                 qint64 mtime_sec)
{
  if (!search_session_.active) {
    return;
  }
  // Synthetic: size/mtime already collected on the worker via directory_entry.
  // Avoid FileInfo::from_path (stat) on the GUI thread for every hit.
  const std::filesystem::path p{path.toStdString()};
  auto info = fs::FileInfo::synthetic(fs::Location::from_path(p), p.filename().string(),
                                      is_directory, size);
  if (mtime_sec > 0) {
    info.set_mtime_unix(static_cast<std::int64_t>(mtime_sec));
  }
  search_session_.results.push_back(info);
  search_session_.batch.push_back(std::move(info));
  // Incremental inserts (batched) beat periodic full layoutChanged on large sets.
  // Early results flush soon for responsiveness; larger batches later reduce model churn.
  if (search_session_.batch.size() >= 48 || search_session_.results.size() < 24) {
    flush_search_batch();
  } else if (search_session_.batch.size() == 1) {
    // Schedule a deferred flush so a trickle of hits still paints without waiting
    // for a full batch (or search end).
    QTimer::singleShot(50, this, [this] {
      if (search_session_.active) {
        flush_search_batch();
      }
    });
  }
}

void MainWindow::flush_search_batch()
{
  if (search_session_.batch.empty()) {
    return;
  }
  const int count = static_cast<int>(search_session_.batch.size());
  collection_.append_visible_items(std::move(search_session_.batch));
  search_session_.batch.clear();
  if (model_ != nullptr) {
    model_->notify_rows_appended(count);
  } else {
    refresh_list();
  }
  update_status_selection();
  // Search hits arrive as synthetic FileInfos; queue thumbs as rows appear so
  // Icons/Detail are not blank until the whole search finishes.
  request_thumbnails_for_visible();
}

void MainWindow::on_search_progress(quint64 visited, quint64 matched)
{
  (void)visited;
  // Throttle status updates — progress can fire very frequently on huge trees.
  if (matched == search_session_.status_matched) {
    return;
  }
  if (matched < 32 || matched - search_session_.status_matched >= 32) {
    search_session_.status_matched = matched;
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
  // Keep search_session_.active true so directory watcher does not wipe results until user navigates.
}

void MainWindow::on_clear_filter()
{
  // Escape dismisses LeapWidget first so type-ahead search always leaves cleanly
  // (global Escape shortcut can intercept before the overlay's own Key_Escape handler).
  if (leap_widget_ != nullptr && leap_widget_->isVisible()) {
    leap_widget_->clear();
    leap_widget_->hide();
    if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
      graphics_view_->setFocus(Qt::OtherFocusReason);
    } else if (QAbstractItemView* view = current_view()) {
      view->setFocus(Qt::OtherFocusReason);
    }
    return;
  }
  const bool search_visible =
      filter_search_.search_row() != nullptr
          ? filter_search_.search_row()->isVisible()
          : (filter_search_.search_edit() != nullptr && filter_search_.search_edit()->isVisible());
  if (search_visible) {
    stop_search();
    search_session_.active = false;
    search_session_.results.clear();
    filter_search_.set_search_visible(false);
    filter_search_.clear_search();
    on_directory_changed();
    // Restore focus so leap / type-ahead work again.
    if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
      graphics_view_->setFocus(Qt::OtherFocusReason);
    } else if (QAbstractItemView* view = current_view()) {
      view->setFocus(Qt::OtherFocusReason);
    }
    return;
  }
  if (!filter_search_.filter_text().isEmpty()) {
    filter_search_.clear_filter();
    return;
  }
  const bool filter_visible =
      filter_search_.filter_row() != nullptr
          ? filter_search_.filter_row()->isVisible()
          : (filter_search_.filter_edit() != nullptr && filter_search_.filter_edit()->isVisible());
  if (filter_visible && !filter_pinned_ && show_filter_act_ != nullptr
      && show_filter_act_->isChecked()) {
    show_filter_act_->setChecked(false);
    // Hide leaves focus on a now-hidden widget; put it back on the file view
    // so leap search (type-ahead) can be activated again.
    if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
      graphics_view_->setFocus(Qt::OtherFocusReason);
    } else if (QAbstractItemView* view = current_view()) {
      view->setFocus(Qt::OtherFocusReason);
    }
    return;
  }
  if (location_chrome_.line_edit_visible()) {
    show_location_buttons();
  }
}


} // namespace dirtoo::app

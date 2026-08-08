// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include "file_context_menu.hpp"
#include "properties_dialog.hpp"
#include "open_with.hpp"
#include "about_dialog.hpp"
#include "size_format.hpp"
#include "clipboard.hpp"
#include <QFileDialog>
#include <QMimeDatabase>
#include <QScrollBar>
#include <QTextStream>
#include <QFile>
#include <QDesktopServices>
#include <QUrl>
#include <filesystem>
#include <set>

namespace dirtoo::app {

void MainWindow::on_item_activated(const QModelIndex& index)
{
  // Copy FileInfo by value immediately. Async filter/sort can replace the
  // visible list between the click and this slot; holding a pointer/row would
  // open the wrong folder or file (especially under an active filter).
  if (model_ == nullptr) {
    return;
  }
  const fs::FileInfo* fi_ptr = model_->file_at(index.row());
  if (fi_ptr == nullptr) {
    return;
  }
  const fs::FileInfo fi = *fi_ptr;
  if (fi.is_directory()) {
    if (location_.is_archive()) {
      open_location(location_.join(fi.basename()));
    } else {
      open_location(fi.location());
    }
  } else if (location_.is_archive()) {
    // Extract single member then open with the default application.
    const auto member = location_.entry_path().empty()
                            ? std::filesystem::path{fi.basename()}
                            : location_.entry_path() / fi.basename();
    const auto cache = std::filesystem::temp_directory_path() / "dirtoo-open" /
                       std::to_string(std::hash<std::string>{}(location_.as_path().string()));
    auto extracted = archive::extract_member(location_.as_path(), member, cache);
    if (!extracted) {
      QMessageBox::warning(this, QStringLiteral("Archive"),
                           QString::fromStdString(extracted.error()));
      return;
    }
    if (fs::looks_like_archive(*extracted)) {
      open_location(fs::Location::from_archive(*extracted, {}));
    } else {
      open_default(*extracted);
    }
  } else if (fs::looks_like_archive(fi.path())) {
    open_location(fs::Location::from_archive(fi.path(), {}));
  } else {
    open_default(fi.path());
  }
}

std::vector<fs::FileInfo> MainWindow::selected_fileinfos() const
{
  if (model_ == nullptr) {
    return {};
  }
  if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
    std::vector<fs::FileInfo> out;
    for (int row : graphics_view_->selected_rows()) {
      if (const auto* fi = model_->file_at(row)) {
        out.push_back(*fi);
      }
    }
    return out;
  }
  auto* view = current_view();
  if (view == nullptr || view->selectionModel() == nullptr) {
    return {};
  }
  return model_->files_at(view->selectionModel()->selectedIndexes());
}


void MainWindow::on_toggle_show_abspath(bool checked)
{
  show_abspath_ = checked;
  if (model_ != nullptr) {
    model_->set_show_abspath(checked);
  }
  if (graphics_view_ != nullptr) {
    graphics_view_->viewport()->update();
  }
}

void MainWindow::refresh_list()
{
  model_->refresh();
  update_status_selection();
}

void MainWindow::on_properties()
{
  const auto selected = selected_fileinfos();
  if (selected.empty()) {
    set_status(QStringLiteral("Nothing selected"));
    return;
  }
  show_properties_dialog(this, selected);
}

void MainWindow::on_selection_changed()
{
  update_status_selection();
}

void MainWindow::update_status_selection()
{
  // Left: filename (or multi-selection summary). Right: directory size stats
  // (sum of listed item sizes — same idea as dirtoo-py controller._update_info).
  const auto& all = collection_.items();
  const auto& visible = collection_.visible_items();
  const auto selected = selected_fileinfos();

  std::uint64_t visible_bytes = 0;
  for (const auto& fi : visible) {
    visible_bytes += fi.size();
  }
  std::uint64_t total_bytes = 0;
  for (const auto& fi : all) {
    total_bytes += fi.size();
  }

  QString info = QStringLiteral("%1 visible (%2)")
                     .arg(visible.size())
                     .arg(format_byte_size(visible_bytes));
  if (all.size() != visible.size()) {
    info += QStringLiteral(", %1 total (%2)")
                .arg(all.size())
                .arg(format_byte_size(total_bytes));
  }

  if (!selected.empty()) {
    std::uint64_t selected_bytes = 0;
    for (const auto& fi : selected) {
      selected_bytes += fi.size();
    }
    info += QStringLiteral(", %1 selected (%2)")
                .arg(selected.size())
                .arg(format_byte_size(selected_bytes));
  }

  // Thumbnail / meta progress for currently tracked paths.
  if (model_ != nullptr) {
    const auto tc = model_->thumbnail_counts();
    const int tracked = tc.pending + tc.ready + tc.failed;
    if (tc.pending > 0) {
      info += QStringLiteral(", thumbs %1/%2")
                  .arg(tc.ready)
                  .arg(tracked);
    } else if (tracked > 0 && tc.ready > 0) {
      // Brief steady-state: omit once everything is done to keep the bar quiet.
    }
  }

  if (status_info_label_ != nullptr) {
    status_info_label_->setText(QStringLiteral("  ") + info);
  }

  if (selected.empty()) {
    // Clear left so transient set_status messages (Loading…, etc.) can show;
    // steady state with no selection leaves the right panel as the summary.
    if (status_label_ != nullptr) {
      status_label_->setText(QString());
    }
  } else if (selected.size() == 1) {
    set_status(QString::fromStdString(selected.front().path().string()));
  } else {
    set_status(QStringLiteral("%1 selected").arg(selected.size()));
  }
}

void MainWindow::on_urls_dropped(const QList<QUrl>& urls, Qt::DropAction action)
{
  on_urls_dropped_to(urls, action, {});
}

void MainWindow::on_select_all()
{
  if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
    // Must mark all model rows, not only live viewport tiles.
    graphics_view_->select_all();
    return;
  }
  if (QAbstractItemView* view = current_view()) {
    view->selectAll();
  }
}

void MainWindow::on_save_file_list()
{
  if (model_ == nullptr) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Save File List As"),
      QString::fromStdString(location_.as_path().string()) + QStringLiteral("/filelist.txt"),
      QStringLiteral("Text files (*.txt);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
    QMessageBox::warning(this, QStringLiteral("Save File List"),
                         QStringLiteral("Could not write %1").arg(path));
    return;
  }
  QTextStream out(&file);
  const int rows = model_->rowCount();
  for (int r = 0; r < rows; ++r) {
    if (const auto* fi = model_->file_at(r)) {
      out << QString::fromStdString(fi->path().string()) << QChar('\n');
    }
  }
  set_status(QStringLiteral("Saved %1 paths to %2").arg(rows).arg(path));
}

void MainWindow::on_open_with()
{
  const auto selected = selected_fileinfos();
  if (selected.empty()) {
    set_status(QStringLiteral("Nothing selected"));
    return;
  }
  std::vector<std::filesystem::path> paths;
  for (const auto& fi : selected) {
    paths.push_back(fi.path());
  }
  // Prefer a listed default app when available; otherwise prompt for a command.
  QMimeDatabase db;
  const QMimeType mt = db.mimeTypeForFile(QString::fromStdString(paths.front().string()),
                                          QMimeDatabase::MatchExtension);
  const auto apps = apps_for_mime(mt.isValid() ? mt.name() : QStringLiteral("application/octet-stream"));
  if (!apps.empty()) {
    if (launch_desktop_app(apps.front(), paths)) {
      return;
    }
  }
  open_with_command_dialog(this, paths);
}

void MainWindow::on_open_terminal()
{
  std::filesystem::path dir = location_.as_path();
  const auto selected = selected_fileinfos();
  if (selected.size() == 1 && selected.front().is_directory()) {
    dir = selected.front().path();
  }
  if (!open_in_terminal(dir)) {
    set_status(QStringLiteral("Could not launch a terminal emulator"));
  }
}

void MainWindow::on_toggle_hidden(bool checked)
{
  const QString expr = filter_edit_ != nullptr ? filter_edit_->text() : QString();
  if (filter_expression_needs_content_io(expr)) {
    collection_.set_show_hidden(checked, false);
  if (sidebar_places_.model() != nullptr) {
    sidebar_places_.set_show_hidden(checked);
  }
    request_async_filter();
    return;
  }
  collection_.set_show_hidden(checked);
  if (sidebar_places_.model() != nullptr) {
    sidebar_places_.set_show_hidden(checked);
  }
  refresh_list();
  request_thumbnails_for_visible();
}

void MainWindow::on_about()
{
  show_about_dialog(this);
}

void MainWindow::on_view_middle_click(const QModelIndex& index)
{
  const fs::FileInfo* fi = model_->file_at(index.row());
  if (fi == nullptr) {
    return;
  }
  if (fi->is_directory()) {
    if (location_.is_archive()) {
      open_new_window(location_.join(fi->basename()));
    } else {
      open_new_window(fi->location());
    }
  } else if (fs::looks_like_archive(fi->path()) && !location_.is_archive()) {
    open_new_window(fs::Location::from_archive(fi->path(), {}));
  }
}

void MainWindow::on_show_leap()
{
  if (leap_widget_ != nullptr) {
    leap_widget_->show_and_focus();
  }
}

void MainWindow::jump_to_row(int row)
{
  if (model_ == nullptr || row < 0 || row >= model_->rowCount()) {
    return;
  }
  const QModelIndex idx = model_->index(row, 0);
  if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
    graphics_view_->select_row(row, true);
    // select_row materializes the item when needed; centre it in the viewport.
    const auto items = graphics_view_->scene()->selectedItems();
    if (!items.isEmpty()) {
      graphics_view_->ensureVisible(items.front(), 32, 32);
    } else if (row == 0) {
      graphics_view_->verticalScrollBar()->setValue(0);
    } else if (row >= model_->rowCount() - 1) {
      graphics_view_->verticalScrollBar()->setValue(
          graphics_view_->verticalScrollBar()->maximum());
    }
    return;
  }
  auto* view = current_view();
  if (view == nullptr || view->selectionModel() == nullptr) {
    return;
  }
  view->setCurrentIndex(idx);
  view->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  view->scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

} // namespace dirtoo::app

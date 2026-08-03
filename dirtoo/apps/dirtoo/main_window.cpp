// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"
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

void MainWindow::request_async_sort()
{
  // Content filters own visible_ via FilterWorker. Sorting must not call
  // replace_items_sorted → rebuild_visible (GUI content I/O / wipe filter).
  if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()
      && filter_expression_needs_content_io(filter_edit_->text())) {
    collection_.sort_items_only();
    request_async_filter(/*keep_previous_visible=*/true);
    return;
  }
  if (sort_worker_ == nullptr) {
    collection_.apply_sort();
    refresh_list();
    return;
  }
  const quint64 gen = ++sort_generation_;
  auto items = collection_.items(); // copy
  const auto key = collection_.sorter().key();
  const bool asc = collection_.sorter().ascending();
  const bool dirs_first = collection_.sorter().directories_first();
  QMetaObject::invokeMethod(sort_worker_, "sort_items", Qt::QueuedConnection,
                            Q_ARG(std::vector<dirtoo::fs::FileInfo>, items),
                            Q_ARG(dirtoo::collection::SortKey, key), Q_ARG(bool, asc),
                            Q_ARG(bool, dirs_first), Q_ARG(quint64, gen));
}

void MainWindow::on_sort_finished(quint64 generation, std::vector<fs::FileInfo> items)
{
  if (generation != sort_generation_ || search_active_) {
    return;
  }
  // match_/filter and show_hidden stay; only item order changes.
  collection_.replace_items_sorted(std::move(items));
  refresh_list();
  set_status(QStringLiteral("%1 items").arg(collection_.visible_items().size()));
  request_thumbnails_for_visible();
}

void MainWindow::apply_sort_key(collection::SortKey key, bool toggle_if_same)
{
  if (toggle_if_same && sort_key_ == key) {
    sort_ascending_ = !sort_ascending_;
  } else {
    sort_key_ = key;
    sort_ascending_ = true;
  }
  collection_.sorter().set_key(key);
  collection_.sorter().set_ascending(sort_ascending_);
  update_sort_toolbar_label();
  if (tree_view_ != nullptr && tree_view_->header() != nullptr) {
    tree_view_->header()->setSortIndicatorShown(true);
    // Map SortKey → visible FileListColumn for indicator when possible.
    int section = static_cast<int>(FileListColumn::Name);
    switch (key) {
    case collection::SortKey::Size:
      section = static_cast<int>(FileListColumn::Size);
      break;
    case collection::SortKey::Width:
      section = static_cast<int>(FileListColumn::Width);
      break;
    case collection::SortKey::Height:
      section = static_cast<int>(FileListColumn::Height);
      break;
    case collection::SortKey::Resolution:
      section = static_cast<int>(FileListColumn::Dimensions);
      break;
    case collection::SortKey::AspectRatio:
      section = static_cast<int>(FileListColumn::AspectRatio);
      break;
    case collection::SortKey::Framerate:
      section = static_cast<int>(FileListColumn::Framerate);
      break;
    case collection::SortKey::Duration:
      section = static_cast<int>(FileListColumn::Duration);
      break;
    case collection::SortKey::Modified:
      section = static_cast<int>(FileListColumn::Modified);
      break;
    case collection::SortKey::Type:
    case collection::SortKey::Extension:
      section = static_cast<int>(FileListColumn::Type);
      break;
    default:
      section = static_cast<int>(FileListColumn::Name);
      break;
    }
    tree_view_->header()->setSortIndicator(
        section, sort_ascending_ ? Qt::AscendingOrder : Qt::DescendingOrder);
  }
  request_async_sort();
}

void MainWindow::update_sort_toolbar_label()
{
  if (sort_toolbar_btn_ == nullptr) {
    return;
  }
  QString title = QStringLiteral("Name");
  switch (sort_key_) {
  case collection::SortKey::Size:
    title = QStringLiteral("Size");
    break;
  case collection::SortKey::Modified:
    title = QStringLiteral("Modified");
    break;
  case collection::SortKey::Type:
  case collection::SortKey::Extension:
    title = QStringLiteral("Type");
    break;
  case collection::SortKey::Width:
    title = QStringLiteral("Width");
    break;
  case collection::SortKey::Height:
    title = QStringLiteral("Height");
    break;
  case collection::SortKey::Resolution:
    title = QStringLiteral("Dimensions");
    break;
  case collection::SortKey::AspectRatio:
    title = QStringLiteral("Aspect");
    break;
  case collection::SortKey::Duration:
    title = QStringLiteral("Duration");
    break;
  case collection::SortKey::Framerate:
    title = QStringLiteral("FPS");
    break;
  case collection::SortKey::Random:
    title = QStringLiteral("Random");
    break;
  default:
    title = QStringLiteral("Name");
    break;
  }
  sort_toolbar_btn_->setText(title);
}

void MainWindow::on_header_clicked(int section)
{
  using collection::SortKey;
  SortKey key = SortKey::Name;
  switch (static_cast<FileListColumn>(section)) {
  case FileListColumn::Name:
    key = SortKey::Name;
    break;
  case FileListColumn::Size:
    key = SortKey::Size;
    break;
  case FileListColumn::Width:
    key = SortKey::Width;
    break;
  case FileListColumn::Height:
    key = SortKey::Height;
    break;
  case FileListColumn::Dimensions:
    key = SortKey::Resolution; // width * height
    break;
  case FileListColumn::AspectRatio:
    key = SortKey::AspectRatio;
    break;
  case FileListColumn::Framerate:
    key = SortKey::Framerate;
    break;
  case FileListColumn::Duration:
    key = SortKey::Duration;
    break;
  case FileListColumn::Modified:
  case FileListColumn::Accessed:
  case FileListColumn::Changed:
  case FileListColumn::Birth:
    // Extra time columns share Modified sort until dedicated SortKeys exist.
    key = SortKey::Modified;
    break;
  case FileListColumn::Type:
    key = SortKey::Type;
    break;
  case FileListColumn::Count:
    return;
  }
  apply_sort_key(key, /*toggle_if_same=*/true);
}

void MainWindow::on_item_activated(const QModelIndex& index)
{
  const fs::FileInfo* fi = model_->file_at(index.row());
  if (fi == nullptr) {
    return;
  }
  if (fi->is_directory()) {
    if (location_.is_archive()) {
      open_location(location_.join(fi->basename()));
    } else {
      open_location(fi->location());
    }
  } else if (location_.is_archive()) {
    // Extract single member then open with the default application.
    const auto member = location_.entry_path().empty()
                            ? std::filesystem::path{fi->basename()}
                            : location_.entry_path() / fi->basename();
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
  } else if (fs::looks_like_archive(fi->path())) {
    open_location(fs::Location::from_archive(fi->path(), {}));
  } else {
    open_default(fi->path());
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

void MainWindow::on_context_menu(const QPoint& pos)
{
  auto* view = current_view();
  const bool graphics = (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr);
  if (view == nullptr && !graphics) {
    return;
  }

  // Resolve the item under the pointer (if any). Empty background → directory menu.
  QModelIndex under;
  if (graphics && graphics_view_ != nullptr) {
    under = graphics_view_->index_at(pos);
    if (under.isValid()) {
      const auto rows = graphics_view_->selected_rows();
      bool already = false;
      for (int r : rows) {
        if (r == under.row()) {
          already = true;
          break;
        }
      }
      if (!already) {
        graphics_view_->select_row(under.row(), true);
      }
    }
  } else if (view != nullptr && view->selectionModel() != nullptr) {
    const QPoint vp = view->viewport()->mapFrom(view, pos);
    under = view->indexAt(vp);
    if (under.isValid() && !view->selectionModel()->isSelected(under)) {
      view->selectionModel()->select(
          under, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
      view->setCurrentIndex(under);
    }
  }

  FileContextMenuCallbacks cb;
  cb.current_location = location_;
  cb.mkdir = [this] { on_mkdir(); };
  cb.create_file = [this] { on_create_file(); };
  cb.paste = [this] { on_paste(); };
  cb.select_all = [this] { on_select_all(); };
  cb.cut = [this] { on_cut(); };
  cb.copy = [this] { on_copy(); };
  cb.delete_selected = [this] { on_delete_selected(); };
  cb.rename_selected = [this] { on_rename_selected(); };
  cb.properties_selected = [this] { on_properties(); };
  cb.reload_thumbnails = [this] { on_reload_thumbnails(); };
  cb.prepare_thumbnails = [this] { on_prepare_thumbnails(); };
  cb.make_directory_thumbnails = [this] { on_make_directory_thumbnails(); };
  cb.open_location = [this](const fs::Location& loc) { open_location(loc); };
  cb.open_location_new_window = [this](const fs::Location& loc) {
    auto* win = new MainWindow();
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->show();
    win->open_location(loc);
  };
  cb.open_terminal = [this](const std::filesystem::path& dir) {
    if (!open_in_terminal(dir)) {
      set_status(QStringLiteral("Could not launch a terminal emulator"));
    }
  };
  cb.paste_into = [this](const std::filesystem::path& dest) {
    const ClipboardPayload payload =
        parse_clipboard_mime(QApplication::clipboard()->mimeData());
    if (payload.paths.empty()) {
      set_status(QStringLiteral("Clipboard has no files"));
      return;
    }
    TransferRequest req;
    req.mode = payload.mode;
    req.sources = payload.paths;
    req.destination_directory = dest;
    start_transfer(req);
  };
  cb.set_status = [this](const QString& s) { set_status(s); };
  cb.show_properties = [this](const std::vector<fs::FileInfo>& items) {
    show_properties_dialog(this, items);
  };

  const QPoint global = graphics ? graphics_view_->mapToGlobal(pos)
                                 : view->viewport()->mapToGlobal(pos);
  if (!under.isValid()) {
    exec_directory_context_menu(this, global, cb);
  } else {
    exec_item_context_menu(this, global, selected_fileinfos(), cb);
  }
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
  if (directory_tree_model_ != nullptr) {
    directory_tree_model_->set_show_hidden(checked);
  }
    request_async_filter();
    return;
  }
  collection_.set_show_hidden(checked);
  if (directory_tree_model_ != nullptr) {
    directory_tree_model_->set_show_hidden(checked);
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

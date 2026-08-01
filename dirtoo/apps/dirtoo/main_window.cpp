// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"

#include "clipboard.hpp"
#include "conflict_dialog.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirops/ops.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPixmap>
#include <QStackedWidget>
#include <QToolBar>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

#include <filesystem>

namespace dirtoo::app {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
  setWindowTitle(QStringLiteral("dirtoo"));
  resize(960, 640);

  // Background transfer worker
  transfer_worker_ = new TransferWorker;
  transfer_worker_->moveToThread(&transfer_thread_);
  connect(&transfer_thread_, &QThread::finished, transfer_worker_, &QObject::deleteLater);
  connect(transfer_worker_, &TransferWorker::item_started, this,
          &MainWindow::on_transfer_item_started);
  connect(transfer_worker_, &TransferWorker::byte_progress, this,
          &MainWindow::on_transfer_byte_progress);
  connect(transfer_worker_, &TransferWorker::conflict_required, this,
          &MainWindow::on_transfer_conflict);
  connect(transfer_worker_, &TransferWorker::finished, this, &MainWindow::on_transfer_finished);
  transfer_thread_.start();

  auto* toolbar = addToolBar(QStringLiteral("Main"));
  toolbar->setMovable(false);

  back_act_ = toolbar->addAction(QStringLiteral("Back"), this, &MainWindow::on_go_back);
  forward_act_ = toolbar->addAction(QStringLiteral("Forward"), this, &MainWindow::on_go_forward);
  toolbar->addAction(QStringLiteral("Parent"), this, &MainWindow::on_go_parent);
  toolbar->addAction(QStringLiteral("Home"), this, &MainWindow::on_go_home);
  toolbar->addSeparator();
  toolbar->addAction(QStringLiteral("New Folder"), this, &MainWindow::on_mkdir);
  toolbar->addSeparator();
  toolbar->addAction(QStringLiteral("Cut"), this, &MainWindow::on_cut);
  toolbar->addAction(QStringLiteral("Copy"), this, &MainWindow::on_copy);
  paste_act_ = toolbar->addAction(QStringLiteral("Paste"), this, &MainWindow::on_paste);
  toolbar->addSeparator();

  auto* view_group = new QActionGroup(this);
  detail_act_ = toolbar->addAction(QStringLiteral("Detail"));
  icons_act_ = toolbar->addAction(QStringLiteral("Icons"));
  detail_act_->setCheckable(true);
  icons_act_->setCheckable(true);
  detail_act_->setChecked(true);
  view_group->addAction(detail_act_);
  view_group->addAction(icons_act_);
  connect(detail_act_, &QAction::triggered, this, &MainWindow::on_view_detail);
  connect(icons_act_, &QAction::triggered, this, &MainWindow::on_view_icons);

  toolbar->addSeparator();
  toolbar->addAction(QStringLiteral("Zoom −"), this, &MainWindow::on_zoom_out);
  toolbar->addAction(QStringLiteral("Zoom +"), this, &MainWindow::on_zoom_in);

  {
    auto* cut = new QAction(this);
    cut->setShortcut(QKeySequence::Cut);
    connect(cut, &QAction::triggered, this, &MainWindow::on_cut);
    addAction(cut);
    auto* copy = new QAction(this);
    copy->setShortcut(QKeySequence::Copy);
    connect(copy, &QAction::triggered, this, &MainWindow::on_copy);
    addAction(copy);
    auto* paste = new QAction(this);
    paste->setShortcut(QKeySequence::Paste);
    connect(paste, &QAction::triggered, this, &MainWindow::on_paste);
    addAction(paste);
    auto* del = new QAction(this);
    del->setShortcut(QKeySequence::Delete);
    connect(del, &QAction::triggered, this, &MainWindow::on_delete_selected);
    addAction(del);
    auto* zoomin = new QAction(this);
    zoomin->setShortcut(QKeySequence::ZoomIn);
    connect(zoomin, &QAction::triggered, this, &MainWindow::on_zoom_in);
    addAction(zoomin);
    auto* zoomout = new QAction(this);
    zoomout->setShortcut(QKeySequence::ZoomOut);
    connect(zoomout, &QAction::triggered, this, &MainWindow::on_zoom_out);
    addAction(zoomout);
  }

  auto* central = new QWidget(this);
  auto* layout = new QVBoxLayout(central);

  location_edit_ = new QLineEdit(central);
  location_edit_->setPlaceholderText(QStringLiteral("Location"));
  connect(location_edit_, &QLineEdit::returnPressed, this, &MainWindow::on_location_entered);
  layout->addWidget(location_edit_);

  filter_edit_ = new QLineEdit(central);
  filter_edit_->setPlaceholderText(QStringLiteral("Filter by name…"));
  connect(filter_edit_, &QLineEdit::textChanged, this, &MainWindow::on_filter_changed);
  layout->addWidget(filter_edit_);

  model_ = new FileListModel(this);
  model_->set_collection(&collection_);

  view_stack_ = new QStackedWidget(central);

  tree_view_ = new QTreeView(view_stack_);
  tree_view_->setModel(model_);
  tree_view_->setRootIsDecorated(false);
  tree_view_->setUniformRowHeights(true);
  tree_view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  tree_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  tree_view_->setSortingEnabled(false);
  tree_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  tree_view_->setIconSize(QSize(24, 24));
  tree_view_->header()->setStretchLastSection(true);
  tree_view_->setColumnWidth(0, 320);
  tree_view_->setColumnWidth(1, 100);
  tree_view_->setColumnWidth(2, 160);
  connect(tree_view_, &QTreeView::activated, this, &MainWindow::on_item_activated);
  connect(tree_view_, &QWidget::customContextMenuRequested, this, &MainWindow::on_context_menu);
  connect(tree_view_->header(), &QHeaderView::sectionClicked, this, &MainWindow::on_header_clicked);
  view_stack_->addWidget(tree_view_);

  icon_view_ = new QListView(view_stack_);
  icon_view_->setModel(model_);
  icon_view_->setViewMode(QListView::IconMode);
  icon_view_->setResizeMode(QListView::Adjust);
  icon_view_->setMovement(QListView::Static);
  icon_view_->setUniformItemSizes(true);
  icon_view_->setWordWrap(true);
  icon_view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  icon_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(icon_view_, &QListView::activated, this, &MainWindow::on_item_activated);
  connect(icon_view_, &QWidget::customContextMenuRequested, this, &MainWindow::on_context_menu);
  view_stack_->addWidget(icon_view_);
  apply_icon_zoom();

  layout->addWidget(view_stack_, 1);

  status_label_ = new QLabel(central);
  layout->addWidget(status_label_);
  setCentralWidget(central);

  connect(&watcher_, &watcher::DirectoryWatcher::directory_changed, this,
          &MainWindow::on_directory_changed);
  connect(&watcher_, &watcher::DirectoryWatcher::message, this, [this](const QString& msg) {
    status_label_->setText(msg);
  });

  connect(&thumbnailer_, &thumbnail::Thumbnailer::thumbnail_ready, this,
          &MainWindow::on_thumbnail_ready);
  connect(&thumbnailer_, &thumbnail::Thumbnailer::thumbnail_failed, this,
          &MainWindow::on_thumbnail_failed);

  connect(qApp->clipboard(), &QClipboard::dataChanged, this, &MainWindow::update_edit_actions);

  update_history_actions();
  update_edit_actions();
  set_view_mode(ViewMode::Detail);
}

MainWindow::~MainWindow()
{
  if (transfer_worker_ != nullptr) {
    QMetaObject::invokeMethod(transfer_worker_, &TransferWorker::cancel, Qt::QueuedConnection);
  }
  transfer_thread_.quit();
  transfer_thread_.wait(5000);
}

QAbstractItemView* MainWindow::current_view() const
{
  return view_mode_ == ViewMode::Detail ? static_cast<QAbstractItemView*>(tree_view_)
                                        : static_cast<QAbstractItemView*>(icon_view_);
}

void MainWindow::apply_icon_zoom()
{
  const int size = kZoomLevels[zoom_index_];
  icon_view_->setIconSize(QSize(size, size));
  icon_view_->setGridSize(QSize(size + 28, size + 48));
  // Detail view keeps small icons; slightly scale with zoom for consistency.
  const int detail = std::max(16, size / 4);
  tree_view_->setIconSize(QSize(detail, detail));
}

void MainWindow::on_zoom_in()
{
  if (zoom_index_ + 1 < static_cast<int>(std::size(kZoomLevels))) {
    ++zoom_index_;
    apply_icon_zoom();
  }
}

void MainWindow::on_zoom_out()
{
  if (zoom_index_ > 0) {
    --zoom_index_;
    apply_icon_zoom();
  }
}

void MainWindow::set_view_mode(ViewMode mode)
{
  view_mode_ = mode;
  if (mode == ViewMode::Detail) {
    view_stack_->setCurrentWidget(tree_view_);
    detail_act_->setChecked(true);
  } else {
    view_stack_->setCurrentWidget(icon_view_);
    icons_act_->setChecked(true);
    request_thumbnails_for_visible();
  }
}

void MainWindow::on_view_detail()
{
  set_view_mode(ViewMode::Detail);
}

void MainWindow::on_view_icons()
{
  set_view_mode(ViewMode::Icons);
}

void MainWindow::open_location(const fs::Location& location, bool record_history)
{
  location_ = location;
  location_edit_->setText(QString::fromStdString(location_.as_path().string()));

  if (record_history) {
    if (history_index_ >= 0 && history_index_ + 1 < static_cast<int>(history_.size())) {
      history_.erase(history_.begin() + history_index_ + 1, history_.end());
    }
    if (history_.empty() || history_.back().as_path() != location.as_path()) {
      history_.push_back(location);
      history_index_ = static_cast<int>(history_.size()) - 1;
    } else {
      history_index_ = static_cast<int>(history_.size()) - 1;
    }
  }
  update_history_actions();

  watcher_.set_location(location_);
  watcher_.start();
}

void MainWindow::on_location_entered()
{
  try {
    open_location(fs::Location::from_human(location_edit_->text().toStdString()));
  } catch (const std::exception& ex) {
    status_label_->setText(QString::fromUtf8(ex.what()));
  }
}

void MainWindow::on_go_parent()
{
  open_location(location_.parent());
}

void MainWindow::on_go_home()
{
  open_location(fs::Location::from_path(std::filesystem::path{QDir::homePath().toStdString()}));
}

void MainWindow::on_go_back()
{
  if (history_index_ <= 0) {
    return;
  }
  --history_index_;
  open_location(history_[static_cast<std::size_t>(history_index_)], false);
}

void MainWindow::on_go_forward()
{
  if (history_index_ + 1 >= static_cast<int>(history_.size())) {
    return;
  }
  ++history_index_;
  open_location(history_[static_cast<std::size_t>(history_index_)], false);
}

void MainWindow::update_history_actions()
{
  if (back_act_) {
    back_act_->setEnabled(history_index_ > 0);
  }
  if (forward_act_) {
    forward_act_->setEnabled(history_index_ + 1 < static_cast<int>(history_.size()));
  }
}

void MainWindow::update_edit_actions()
{
  if (paste_act_) {
    paste_act_->setEnabled(!transfer_busy_
                           && clipboard_has_paths(QApplication::clipboard()->mimeData()));
  }
}

void MainWindow::on_directory_changed()
{
  model_->clear_thumbnails();
  auto items = fs::list_directory(location_);
  collection_.set_items(std::move(items));

  switch (sort_column_) {
  case SortColumn::Name:
    collection_.sort_by_name(sort_ascending_);
    break;
  case SortColumn::Size:
    collection_.sort_by_size(sort_ascending_);
    break;
  case SortColumn::Modified:
    collection_.sort_by_mtime(sort_ascending_);
    break;
  case SortColumn::Type:
    collection_.sort_by_name(sort_ascending_);
    break;
  }

  if (!filter_edit_->text().isEmpty()) {
    collection_.set_name_filter(filter_edit_->text().toStdString());
  }
  refresh_list();
  request_thumbnails_for_visible();
}

void MainWindow::request_thumbnails_for_visible()
{
  if (view_mode_ != ViewMode::Icons) {
    return;
  }

  QMimeDatabase mime_db;
  std::vector<fs::Location> locs;
  QStringList mimes;
  const auto& visible = collection_.visible_items();
  locs.reserve(visible.size());
  mimes.reserve(static_cast<int>(visible.size()));
  for (const auto& fi : visible) {
    if (fi.is_directory()) {
      continue;
    }
    locs.push_back(fi.location());
    const auto mt = mime_db.mimeTypeForFile(QString::fromStdString(fi.path().string()));
    mimes.push_back(mt.name());
  }
  thumbnailer_.request_many(locs, mimes, QStringLiteral("large"));
}

void MainWindow::on_thumbnail_ready(const fs::Location& location, const QString& path)
{
  QPixmap pix(path);
  if (pix.isNull()) {
    return;
  }
  model_->set_thumbnail(QString::fromStdString(location.as_path().string()), QIcon(pix));
}

void MainWindow::on_thumbnail_failed(const fs::Location& location, const QString& message)
{
  (void)location;
  (void)message;
}

void MainWindow::on_filter_changed(const QString& text)
{
  collection_.set_name_filter(text.toStdString());
  refresh_list();
  request_thumbnails_for_visible();
}

void MainWindow::on_header_clicked(int section)
{
  const auto col = static_cast<SortColumn>(section);
  if (sort_column_ == col) {
    sort_ascending_ = !sort_ascending_;
  } else {
    sort_column_ = col;
    sort_ascending_ = true;
  }
  on_directory_changed();
}

void MainWindow::on_item_activated(const QModelIndex& index)
{
  const fs::FileInfo* fi = model_->file_at(index.row());
  if (fi == nullptr) {
    return;
  }
  if (fi->is_directory()) {
    open_location(fi->location());
  } else {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(fi->path().string())));
  }
}

std::vector<fs::FileInfo> MainWindow::selected_fileinfos() const
{
  auto* view = current_view();
  if (view == nullptr || view->selectionModel() == nullptr) {
    return {};
  }
  return model_->files_at(view->selectionModel()->selectedIndexes());
}

void MainWindow::on_context_menu(const QPoint& pos)
{
  auto* view = current_view();
  QMenu menu(this);
  menu.addAction(QStringLiteral("Open"), this, [this, view] {
    if (view == nullptr || view->selectionModel() == nullptr) {
      return;
    }
    const auto rows = view->selectionModel()->selectedIndexes();
    if (!rows.isEmpty()) {
      on_item_activated(rows.first());
    }
  });
  menu.addSeparator();
  menu.addAction(QStringLiteral("Cut"), this, &MainWindow::on_cut);
  menu.addAction(QStringLiteral("Copy"), this, &MainWindow::on_copy);
  menu.addAction(QStringLiteral("Paste"), this, &MainWindow::on_paste);
  menu.addSeparator();
  menu.addAction(QStringLiteral("Rename…"), this, &MainWindow::on_rename_selected);
  menu.addAction(QStringLiteral("Delete…"), this, &MainWindow::on_delete_selected);
  menu.addSeparator();
  menu.addAction(QStringLiteral("New Folder…"), this, &MainWindow::on_mkdir);
  menu.exec(view->viewport()->mapToGlobal(pos));
}

void MainWindow::set_clipboard(ClipboardMode mode)
{
  const auto selected = selected_fileinfos();
  if (selected.empty()) {
    status_label_->setText(QStringLiteral("Nothing selected"));
    return;
  }
  std::vector<std::filesystem::path> paths;
  paths.reserve(selected.size());
  for (const auto& fi : selected) {
    paths.push_back(fi.path());
  }
  QApplication::clipboard()->setMimeData(make_clipboard_mime(mode, paths));
  status_label_->setText(QStringLiteral("%1 item(s) %2")
                             .arg(paths.size())
                             .arg(mode == ClipboardMode::Cut ? QStringLiteral("cut")
                                                             : QStringLiteral("copied")));
  update_edit_actions();
}

void MainWindow::on_copy()
{
  set_clipboard(ClipboardMode::Copy);
}

void MainWindow::on_cut()
{
  set_clipboard(ClipboardMode::Cut);
}

void MainWindow::start_transfer(const TransferRequest& request)
{
  if (transfer_busy_) {
    status_label_->setText(QStringLiteral("A transfer is already in progress"));
    return;
  }
  transfer_busy_ = true;
  last_transfer_mode_ = request.mode;
  update_edit_actions();

  if (transfer_dialog_ == nullptr) {
    transfer_dialog_ = new TransferDialog(this);
    const auto cancel_worker = [this] {
      if (transfer_worker_ != nullptr) {
        QMetaObject::invokeMethod(transfer_worker_, &TransferWorker::cancel, Qt::QueuedConnection);
      }
    };
    connect(transfer_dialog_, &TransferDialog::cancel_requested, this, cancel_worker);
    connect(transfer_dialog_, &QDialog::rejected, this, cancel_worker);
  }
  transfer_dialog_->reset();
  transfer_dialog_->set_title_text(request.mode == ClipboardMode::Cut ? QStringLiteral("Moving…")
                                                                     : QStringLiteral("Copying…"));
  transfer_dialog_->show();

  QMetaObject::invokeMethod(transfer_worker_, [this, request] { transfer_worker_->run(request); },
                            Qt::QueuedConnection);
}

void MainWindow::on_paste()
{
  if (transfer_busy_) {
    return;
  }

  const ClipboardPayload payload = parse_clipboard_mime(QApplication::clipboard()->mimeData());
  if (payload.paths.empty()) {
    status_label_->setText(QStringLiteral("Clipboard has no files"));
    return;
  }

  TransferRequest req;
  req.mode = payload.mode;
  req.destination_directory = location_.as_path();
  req.sources = payload.paths;
  start_transfer(req);
}

void MainWindow::on_transfer_item_started(int index, int total, const QString& path)
{
  if (transfer_dialog_ != nullptr) {
    transfer_dialog_->set_item_progress(index, total);
    transfer_dialog_->set_current_file(path);
  }
}

void MainWindow::on_transfer_byte_progress(quint64 done, quint64 total, const QString& path)
{
  if (transfer_dialog_ != nullptr) {
    transfer_dialog_->set_current_file(path);
    transfer_dialog_->set_progress(done, total);
  }
}

void MainWindow::on_transfer_conflict(const QString& destination_name)
{
  // Runs on UI thread (QueuedConnection from worker signal).
  const auto chosen = ask_conflict_policy(this, destination_name);
  if (transfer_worker_ == nullptr) {
    return;
  }
  if (!chosen) {
    QMetaObject::invokeMethod(
        transfer_worker_,
        [this] { transfer_worker_->resolve_conflict(dirops::ConflictPolicy::Fail, false); },
        Qt::QueuedConnection);
  } else {
    const auto policy = *chosen;
    QMetaObject::invokeMethod(
        transfer_worker_,
        [this, policy] { transfer_worker_->resolve_conflict(policy, true); },
        Qt::QueuedConnection);
  }
}

void MainWindow::on_transfer_finished(TransferSummary summary)
{
  transfer_busy_ = false;
  if (transfer_dialog_ != nullptr) {
    transfer_dialog_->hide();
  }

  if (!summary.error.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("Transfer"), summary.error);
  }

  if (last_transfer_mode_ == ClipboardMode::Cut && summary.completed > 0 && !summary.cancelled) {
    QApplication::clipboard()->clear();
  }

  if (summary.cancelled) {
    status_label_->setText(QStringLiteral("Transfer cancelled (%1 done, %2 skipped)")
                               .arg(summary.completed)
                               .arg(summary.skipped));
  } else {
    status_label_->setText(QStringLiteral("Transfer: %1 done, %2 skipped")
                               .arg(summary.completed)
                               .arg(summary.skipped));
  }

  on_directory_changed();
  update_edit_actions();
}

void MainWindow::on_mkdir()
{
  bool ok = false;
  const QString name = QInputDialog::getText(this, QStringLiteral("New Folder"),
                                             QStringLiteral("Folder name:"),
                                             QLineEdit::Normal, QStringLiteral("New Folder"), &ok);
  if (!ok || name.trimmed().isEmpty()) {
    return;
  }

  const auto dest = location_.as_path() / name.trimmed().toStdString();
  if (std::filesystem::exists(dest)) {
    const auto chosen = ask_conflict_policy(this, name.trimmed());
    if (!chosen || *chosen == dirops::ConflictPolicy::Skip) {
      return;
    }
    if (*chosen == dirops::ConflictPolicy::Overwrite) {
      auto rm = dirops::remove_path(dest);
      if (!rm) {
        QMessageBox::warning(this, QStringLiteral("New Folder"),
                             QString::fromStdString(rm.error().to_string()));
        return;
      }
    } else if (*chosen == dirops::ConflictPolicy::Rename) {
      const auto unique =
          dest.parent_path() / (dest.stem().string() + " (2)" + dest.extension().string());
      auto result = dirops::create_directory(unique);
      if (!result) {
        QMessageBox::warning(this, QStringLiteral("New Folder"),
                             QString::fromStdString(result.error().to_string()));
      }
      on_directory_changed();
      return;
    }
  }

  auto result = dirops::create_directory(dest);
  if (!result) {
    QMessageBox::warning(this, QStringLiteral("New Folder"),
                         QString::fromStdString(result.error().to_string()));
    return;
  }
  on_directory_changed();
}

void MainWindow::on_rename_selected()
{
  const auto selected = selected_fileinfos();
  if (selected.size() != 1) {
    status_label_->setText(QStringLiteral("Select exactly one item to rename"));
    return;
  }

  const auto& fi = selected.front();
  bool ok = false;
  const QString name = QInputDialog::getText(this, QStringLiteral("Rename"),
                                             QStringLiteral("New name:"), QLineEdit::Normal,
                                             QString::fromStdString(fi.basename()), &ok);
  if (!ok || name.trimmed().isEmpty()) {
    return;
  }

  const auto dest = fi.path().parent_path() / name.trimmed().toStdString();
  dirops::Options opt;
  if (std::filesystem::exists(dest) && dest != fi.path()) {
    const auto chosen = ask_conflict_policy(this, name.trimmed());
    if (!chosen) {
      return;
    }
    opt.conflict = *chosen;
  }

  auto result = dirops::rename_path(fi.path(), dest, opt);
  if (!result) {
    QMessageBox::warning(this, QStringLiteral("Rename"),
                         QString::fromStdString(result.error().to_string()));
    return;
  }
  on_directory_changed();
}

void MainWindow::on_delete_selected()
{
  const auto selected = selected_fileinfos();
  if (selected.empty()) {
    return;
  }

  const QString msg = selected.size() == 1
                          ? QStringLiteral("Delete “%1”?")
                                .arg(QString::fromStdString(selected.front().basename()))
                          : QStringLiteral("Delete %1 items?").arg(selected.size());
  if (QMessageBox::question(this, QStringLiteral("Delete"), msg) != QMessageBox::Yes) {
    return;
  }

  for (const auto& fi : selected) {
    auto result = dirops::remove_path(fi.path());
    if (!result) {
      QMessageBox::warning(this, QStringLiteral("Delete"),
                           QString::fromStdString(result.error().to_string()));
      break;
    }
  }
  on_directory_changed();
}

void MainWindow::refresh_list()
{
  model_->refresh();
  status_label_->setText(QStringLiteral("%1 items in %2")
                             .arg(model_->rowCount())
                             .arg(QString::fromStdString(location_.as_path().string())));
}

} // namespace dirtoo::app

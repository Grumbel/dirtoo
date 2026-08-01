// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"

#include "clipboard.hpp"
#include "conflict_dialog.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirops/ops.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QToolBar>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>

namespace dirtoo::app {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
  setWindowTitle(QStringLiteral("dirtoo"));
  resize(960, 640);

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

  // Standard edit shortcuts
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

  tree_view_ = new QTreeView(central);
  tree_view_->setModel(model_);
  tree_view_->setRootIsDecorated(false);
  tree_view_->setUniformRowHeights(true);
  tree_view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  tree_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  tree_view_->setSortingEnabled(false);
  tree_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  tree_view_->header()->setStretchLastSection(true);
  tree_view_->setColumnWidth(0, 320);
  tree_view_->setColumnWidth(1, 100);
  tree_view_->setColumnWidth(2, 160);
  connect(tree_view_, &QTreeView::activated, this, &MainWindow::on_item_activated);
  connect(tree_view_, &QWidget::customContextMenuRequested, this, &MainWindow::on_context_menu);
  connect(tree_view_->header(), &QHeaderView::sectionClicked, this, &MainWindow::on_header_clicked);
  layout->addWidget(tree_view_, 1);

  status_label_ = new QLabel(central);
  layout->addWidget(status_label_);
  setCentralWidget(central);

  connect(&watcher_, &watcher::DirectoryWatcher::directory_changed, this,
          &MainWindow::on_directory_changed);
  connect(&watcher_, &watcher::DirectoryWatcher::message, this, [this](const QString& msg) {
    status_label_->setText(msg);
  });

  // Keep Paste enabled state roughly in sync.
  connect(qApp->clipboard(), &QClipboard::dataChanged, this, &MainWindow::update_edit_actions);

  update_history_actions();
  update_edit_actions();
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
    paste_act_->setEnabled(clipboard_has_paths(QApplication::clipboard()->mimeData()));
  }
}

void MainWindow::on_directory_changed()
{
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
}

void MainWindow::on_filter_changed(const QString& text)
{
  collection_.set_name_filter(text.toStdString());
  refresh_list();
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
  return model_->files_at(tree_view_->selectionModel()->selectedRows());
}

void MainWindow::on_context_menu(const QPoint& pos)
{
  QMenu menu(this);
  menu.addAction(QStringLiteral("Open"), this, [this] {
    const auto rows = tree_view_->selectionModel()->selectedRows();
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
  menu.exec(tree_view_->viewport()->mapToGlobal(pos));
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

void MainWindow::on_paste()
{
  const ClipboardPayload payload = parse_clipboard_mime(QApplication::clipboard()->mimeData());
  if (payload.paths.empty()) {
    status_label_->setText(QStringLiteral("Clipboard has no files"));
    return;
  }

  const auto dest_dir = location_.as_path();
  int ok_count = 0;
  int skip_count = 0;

  for (const auto& src : payload.paths) {
    const auto dest = dest_dir / src.filename();

    dirops::Options opt;
    if (std::filesystem::exists(dest)) {
      const auto chosen = ask_conflict_policy(this, QString::fromStdString(dest.filename().string()));
      if (!chosen) {
        status_label_->setText(QStringLiteral("Paste cancelled"));
        on_directory_changed();
        return;
      }
      opt.conflict = *chosen;
    } else {
      opt.conflict = dirops::ConflictPolicy::Fail;
    }

    dirops::OpResult result;
    if (payload.mode == ClipboardMode::Cut) {
      result = dirops::move_path(src, dest_dir, opt);
    } else {
      result = dirops::copy_path(src, dest_dir, opt);
    }

    if (!result) {
      QMessageBox::warning(this, QStringLiteral("Paste"),
                           QString::fromStdString(result.error().to_string()));
      break;
    }
    if (!result->items.empty() && result->items.front().skipped) {
      ++skip_count;
    } else {
      ++ok_count;
    }
  }

  // After a successful cut, clear clipboard so items are not moved again.
  if (payload.mode == ClipboardMode::Cut && ok_count > 0) {
    QApplication::clipboard()->clear();
  }

  status_label_->setText(QStringLiteral("Paste: %1 done, %2 skipped")
                             .arg(ok_count)
                             .arg(skip_count));
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
      // create under unique name
      const auto unique = dest.parent_path()
                          / (dest.stem().string() + " (2)" + dest.extension().string());
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

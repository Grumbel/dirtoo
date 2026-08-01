// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"

#include "dirtoo/fs/file_info.hpp"
#include "dirops/ops.hpp"

#include <QAction>
#include <QDesktopServices>
#include <QDir>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QAbstractItemView>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>

namespace dirtoo::app {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
  setWindowTitle(QStringLiteral("dirtoo"));
  resize(900, 600);

  auto* toolbar = addToolBar(QStringLiteral("Main"));
  toolbar->setMovable(false);

  back_act_ = toolbar->addAction(QStringLiteral("Back"));
  connect(back_act_, &QAction::triggered, this, &MainWindow::on_go_back);

  forward_act_ = toolbar->addAction(QStringLiteral("Forward"));
  connect(forward_act_, &QAction::triggered, this, &MainWindow::on_go_forward);

  auto* parent_act = toolbar->addAction(QStringLiteral("Parent"));
  connect(parent_act, &QAction::triggered, this, &MainWindow::on_go_parent);

  auto* home_act = toolbar->addAction(QStringLiteral("Home"));
  connect(home_act, &QAction::triggered, this, &MainWindow::on_go_home);

  toolbar->addSeparator();

  auto* mkdir_act = toolbar->addAction(QStringLiteral("New Folder"));
  connect(mkdir_act, &QAction::triggered, this, &MainWindow::on_mkdir);

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

  list_model_ = new QStringListModel(this);
  list_view_ = new QListView(central);
  list_view_->setModel(list_model_);
  list_view_->setUniformItemSizes(true);
  list_view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  list_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(list_view_, &QListView::activated, this, &MainWindow::on_item_activated);
  connect(list_view_, &QWidget::customContextMenuRequested, this, &MainWindow::on_context_menu);
  layout->addWidget(list_view_, 1);

  status_label_ = new QLabel(central);
  layout->addWidget(status_label_);

  setCentralWidget(central);

  connect(&watcher_, &watcher::DirectoryWatcher::directory_changed, this,
          &MainWindow::on_directory_changed);
  connect(&watcher_, &watcher::DirectoryWatcher::message, this, [this](const QString& msg) {
    status_label_->setText(msg);
  });

  update_history_actions();
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
  const auto home = std::filesystem::path{QDir::homePath().toStdString()};
  open_location(fs::Location::from_path(home));
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

void MainWindow::on_directory_changed()
{
  auto items = fs::list_directory(location_);
  collection_.set_items(std::move(items));
  collection_.sort_by_name(true);
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

void MainWindow::on_item_activated(const QModelIndex& index)
{
  if (!index.isValid()) {
    return;
  }
  const auto& visible = collection_.visible_items();
  const auto i = static_cast<std::size_t>(index.row());
  if (i >= visible.size()) {
    return;
  }
  const auto& fi = visible[i];
  if (fi.is_directory()) {
    open_location(fi.location());
  } else {
    const QUrl url = QUrl::fromLocalFile(QString::fromStdString(fi.path().string()));
    QDesktopServices::openUrl(url);
  }
}

std::vector<fs::FileInfo> MainWindow::selected_fileinfos() const
{
  std::vector<fs::FileInfo> out;
  const auto& visible = collection_.visible_items();
  const auto indexes = list_view_->selectionModel()->selectedIndexes();
  for (const QModelIndex& idx : indexes) {
    const auto i = static_cast<std::size_t>(idx.row());
    if (i < visible.size()) {
      out.push_back(visible[i]);
    }
  }
  return out;
}

void MainWindow::on_context_menu(const QPoint& pos)
{
  QMenu menu(this);
  menu.addAction(QStringLiteral("Open"), this, [this] {
    const auto indexes = list_view_->selectionModel()->selectedIndexes();
    if (!indexes.isEmpty()) {
      on_item_activated(indexes.first());
    }
  });
  menu.addAction(QStringLiteral("Rename…"), this, &MainWindow::on_rename_selected);
  menu.addAction(QStringLiteral("Delete…"), this, &MainWindow::on_delete_selected);
  menu.addSeparator();
  menu.addAction(QStringLiteral("New Folder…"), this, &MainWindow::on_mkdir);
  menu.exec(list_view_->viewport()->mapToGlobal(pos));
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
  auto result = dirops::create_directory(dest);
  if (!result) {
    QMessageBox::warning(this, QStringLiteral("New Folder"),
                         QString::fromStdString(result.error().to_string()));
    return;
  }
  // Watcher should refresh; force a rescan if needed.
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
  auto result = dirops::rename_path(fi.path(), dest);
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
  const auto answer = QMessageBox::question(this, QStringLiteral("Delete"), msg);
  if (answer != QMessageBox::Yes) {
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
  QStringList rows;
  const auto& visible = collection_.visible_items();
  rows.reserve(static_cast<int>(visible.size()));
  for (const auto& fi : visible) {
    QString line = QString::fromStdString(fi.basename());
    if (fi.is_directory()) {
      line += QLatin1Char('/');
    }
    rows.push_back(line);
  }
  list_model_->setStringList(rows);
  status_label_->setText(QStringLiteral("%1 items in %2")
                             .arg(rows.size())
                             .arg(QString::fromStdString(location_.as_path().string())));
}

} // namespace dirtoo::app

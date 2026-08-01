// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"

#include "dirtoo/fs/file_info.hpp"

#include <QAction>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QToolBar>
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

  auto* parent_act = toolbar->addAction(QStringLiteral("Parent"));
  connect(parent_act, &QAction::triggered, this, &MainWindow::on_go_parent);

  auto* home_act = toolbar->addAction(QStringLiteral("Home"));
  connect(home_act, &QAction::triggered, this, &MainWindow::on_go_home);

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
  connect(list_view_, &QListView::activated, this, &MainWindow::on_item_activated);
  layout->addWidget(list_view_, 1);

  status_label_ = new QLabel(central);
  layout->addWidget(status_label_);

  setCentralWidget(central);

  connect(&watcher_, &watcher::DirectoryWatcher::directory_changed, this,
          &MainWindow::on_directory_changed);
  connect(&watcher_, &watcher::DirectoryWatcher::message, this, [this](const QString& msg) {
    status_label_->setText(msg);
  });
}

void MainWindow::open_location(const fs::Location& location)
{
  location_ = location;
  location_edit_->setText(QString::fromStdString(location_.as_path().string()));
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
  }
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
  status_label_->setText(QStringLiteral("%1 items").arg(rows.size()));
}

} // namespace dirtoo::app

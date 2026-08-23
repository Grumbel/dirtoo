// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sidebar_controller.hpp"

#include <QAbstractItemView>
#include <QFrame>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QVBoxLayout>

namespace dirtoo::app {

SidebarController::SidebarController(QObject* parent)
    : QObject(parent)
{
}

QWidget* SidebarController::create(QSplitter* main_splitter)
{
  if (widget_ != nullptr) {
    return widget_;
  }

  splitter_ = main_splitter;
  widget_ = new QWidget(main_splitter);
  auto* sidebar_layout = new QVBoxLayout(widget_);
  sidebar_layout->setContentsMargins(0, 0, 0, 0);
  sidebar_layout->setSpacing(0);

  // Devices panel (top of vertical sidebar splitter).
  auto* devices_panel = new QWidget(widget_);
  auto* devices_layout = new QVBoxLayout(devices_panel);
  devices_layout->setContentsMargins(0, 0, 0, 0);
  devices_layout->setSpacing(0);
  devices_label_ = new QLabel(QStringLiteral("Devices"), devices_panel);
  devices_label_->setStyleSheet(QStringLiteral("font-weight: bold; padding: 4px 6px 2px 6px;"));
  devices_layout->addWidget(devices_label_);
  devices_list_ = new QListWidget(devices_panel);
  devices_list_->setFrameShape(QFrame::NoFrame);
  devices_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  devices_list_->setContextMenuPolicy(Qt::CustomContextMenu);
  devices_layout->addWidget(devices_list_, 1);

  // Places + directory tree (bottom of vertical sidebar splitter).
  auto* places_panel = new QWidget(widget_);
  auto* places_layout = new QVBoxLayout(places_panel);
  places_layout->setContentsMargins(0, 0, 0, 0);
  places_layout->setSpacing(0);
  auto* places_label = new QLabel(QStringLiteral("Places"), places_panel);
  places_label->setStyleSheet(QStringLiteral("font-weight: bold; padding: 6px 6px 2px 6px;"));
  places_layout->addWidget(places_label);

  places_.ensure_model();
  tree_ = new QTreeView(places_panel);
  tree_->setModel(places_.model());
  tree_->setHeaderHidden(true);
  tree_->setUniformRowHeights(true);
  tree_->setAnimated(true);
  tree_->setExpandsOnDoubleClick(true);
  tree_->setFrameShape(QFrame::NoFrame);
  tree_->setSelectionMode(QAbstractItemView::SingleSelection);
  connect(tree_, &QTreeView::activated, this, &SidebarController::on_activated);
  connect(tree_, &QTreeView::clicked, this, &SidebarController::on_activated);
  places_layout->addWidget(tree_, 1);

  auto* sidebar_splitter = new QSplitter(Qt::Vertical, widget_);
  sidebar_splitter->setChildrenCollapsible(false);
  sidebar_splitter->addWidget(devices_panel);
  sidebar_splitter->addWidget(places_panel);
  sidebar_splitter->setStretchFactor(0, 0);
  sidebar_splitter->setStretchFactor(1, 1);
  sidebar_splitter->setSizes({140, 400});
  sidebar_layout->addWidget(sidebar_splitter);

  return widget_;
}

void SidebarController::bind(QWidget* host, QTreeView* tree, QSplitter* main_splitter)
{
  widget_ = host;
  tree_ = tree;
  splitter_ = main_splitter;
}

void SidebarController::set_open_path_handler(std::function<void(const QString& path)> handler)
{
  open_path_ = std::move(handler);
}

void SidebarController::set_visible(bool visible)
{
  if (widget_ != nullptr) {
    widget_->setVisible(visible);
  }
}

void SidebarController::toggle(bool checked)
{
  set_visible(checked);
  if (splitter_ != nullptr && checked && widget_ != nullptr) {
    QList<int> sizes = splitter_->sizes();
    if (sizes.size() >= 2 && sizes[0] < 80) {
      sizes[0] = 220;
      splitter_->setSizes(sizes);
    }
  }
}

void SidebarController::rebuild(const Bookmarks& bookmarks)
{
  places_.rebuild(bookmarks);
}

void SidebarController::sync_to_location(const fs::Location& location)
{
  places_.sync_to_location(tree_, widget_, location);
}

void SidebarController::set_show_hidden(bool show)
{
  if (places_.model() != nullptr) {
    places_.set_show_hidden(show);
  }
}

QString SidebarController::path_for_index(const QModelIndex& index) const
{
  return places_.path_for_index(index);
}

void SidebarController::on_activated(const QModelIndex& index)
{
  emit place_activated(index);
  if (!index.isValid() || !open_path_) {
    return;
  }
  const QString path = places_.path_for_index(index);
  if (path.isEmpty()) {
    return;
  }
  open_path_(path);
}

} // namespace dirtoo::app

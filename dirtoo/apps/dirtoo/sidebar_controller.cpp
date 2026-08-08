// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sidebar_controller.hpp"

#include <QList>

namespace dirtoo::app {

SidebarController::SidebarController(QObject* parent)
    : QObject(parent)
{
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

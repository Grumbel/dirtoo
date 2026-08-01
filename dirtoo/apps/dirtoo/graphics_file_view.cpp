// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "graphics_file_view.hpp"

#include "file_list_model.hpp"
#include "graphics_file_item.hpp"
#include "drag_action_overlay.hpp"

#include <QContextMenuEvent>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <QApplication>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QDrag>

#include <algorithm>

namespace dirtoo::app {

GraphicsFileView::GraphicsFileView(QWidget* parent)
    : QGraphicsView(parent)
{
  scene_ = new QGraphicsScene(this);
  setScene(scene_);
  setAlignment(Qt::AlignLeft | Qt::AlignTop);
  setDragMode(QGraphicsView::RubberBandDrag);
  setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setResizeAnchor(QGraphicsView::AnchorUnderMouse);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setBackgroundBrush(palette().base());
  setAcceptDrops(true);

  connect(scene_, &QGraphicsScene::selectionChanged, this, &GraphicsFileView::on_scene_selection_changed);
}

void GraphicsFileView::set_model(FileListModel* model)
{
  if (model_ == model) {
    return;
  }
  if (model_ != nullptr) {
    disconnect(model_, nullptr, this, nullptr);
  }
  model_ = model;
  if (model_ != nullptr) {
    connect(model_, &QAbstractItemModel::modelReset, this, &GraphicsFileView::on_model_reset);
    connect(model_, &QAbstractItemModel::layoutChanged, this, &GraphicsFileView::on_model_reset);
    connect(model_, &QAbstractItemModel::rowsInserted, this, &GraphicsFileView::on_model_reset);
    connect(model_, &QAbstractItemModel::rowsRemoved, this, &GraphicsFileView::on_model_reset);
    connect(model_, &QAbstractItemModel::dataChanged, this, &GraphicsFileView::on_model_data_changed);
  }
  rebuild_items();
}

void GraphicsFileView::set_tile_size(const QSize& size)
{
  if (tile_size_ == size) {
    return;
  }
  tile_size_ = size;
  for (auto* item : items_) {
    item->set_tile_size(tile_size_);
  }
  layout_items();
}

void GraphicsFileView::set_compact(bool compact)
{
  if (compact_ == compact) {
    return;
  }
  compact_ = compact;
  spacing_ = compact ? 6 : 12;
  layout_items();
}

void GraphicsFileView::relayout()
{
  layout_items();
}

void GraphicsFileView::sync_from_model()
{
  rebuild_items();
}

void GraphicsFileView::rebuild_items()
{
  // Reuse existing QGraphicsItems when the row count changes only slightly —
  // full scene_->clear() + N allocations freezes large directories on every
  // modelReset (load, sort, filter, watcher refresh).
  suppress_selection_signal_ = true;
  if (model_ == nullptr) {
    for (auto* item : items_) {
      scene_->removeItem(item);
      delete item;
    }
    items_.clear();
    suppress_selection_signal_ = false;
    layout_items();
    return;
  }

  const int rows = model_->rowCount();
  const int old_n = static_cast<int>(items_.size());

  // Shrink: drop surplus items from the end.
  if (old_n > rows) {
    for (int i = old_n - 1; i >= rows; --i) {
      auto* item = items_[static_cast<std::size_t>(i)];
      scene_->removeItem(item);
      delete item;
    }
    items_.resize(static_cast<std::size_t>(rows));
  }

  // Update rows already present (path/icon may have changed).
  for (int r = 0; r < static_cast<int>(items_.size()); ++r) {
    items_[static_cast<std::size_t>(r)]->set_row(r);
    items_[static_cast<std::size_t>(r)]->set_tile_size(tile_size_);
    items_[static_cast<std::size_t>(r)]->update();
  }

  // Grow: append new items.
  if (static_cast<int>(items_.size()) < rows) {
    items_.reserve(static_cast<std::size_t>(rows));
    for (int r = static_cast<int>(items_.size()); r < rows; ++r) {
      auto* item = new GraphicsFileItem(model_, r, this);
      item->set_tile_size(tile_size_);
      scene_->addItem(item);
      items_.push_back(item);
    }
  }

  suppress_selection_signal_ = false;
  layout_items();
}

void GraphicsFileView::layout_items()
{
  if (items_.empty()) {
    scene_->setSceneRect(QRectF());
    return;
  }
  const int vp_w = std::max(tile_size_.width() + padding_ * 2,
                            viewport()->width() - 4);
  const int cell_w = tile_size_.width() + spacing_;
  const int cell_h = tile_size_.height() + spacing_;
  int cols = std::max(1, (vp_w - padding_) / cell_w);
  if (compact_) {
    // Prefer wider rows for compact list-like grid
    cols = std::max(1, (vp_w - padding_) / std::max(cell_w, 160));
  }

  // Flow left-to-right. When a group starts mid-row, break to a new row so the
  // section header painted on the first tile of the group has a full line.
  int col = 0;
  int row = 0;
  int max_row = 0;
  for (std::size_t i = 0; i < items_.size(); ++i) {
    if (model_ != nullptr) {
      const QModelIndex idx = model_->index(static_cast<int>(i), 0);
      if (idx.data(IsGroupStartRole).toBool() && i > 0 && col != 0) {
        col = 0;
        ++row;
      }
    }
    items_[i]->setPos(padding_ + col * cell_w, padding_ + row * cell_h);
    max_row = std::max(max_row, row);
    ++col;
    if (col >= cols) {
      col = 0;
      ++row;
    }
  }
  scene_->setSceneRect(0, 0, padding_ * 2 + cols * cell_w, padding_ * 2 + (max_row + 1) * cell_h);
}

QModelIndex GraphicsFileView::index_at(const QPoint& view_pos) const
{
  if (auto* item = qgraphicsitem_cast<GraphicsFileItem*>(itemAt(view_pos))) {
    return item->model_index();
  }
  return {};
}

std::vector<int> GraphicsFileView::selected_rows() const
{
  std::vector<int> rows;
  for (auto* item : items_) {
    if (item->isSelected()) {
      rows.push_back(item->row());
    }
  }
  std::sort(rows.begin(), rows.end());
  return rows;
}

void GraphicsFileView::clear_selection()
{
  scene_->clearSelection();
}

void GraphicsFileView::notify_activated(const QModelIndex& index)
{
  emit activated(index);
}

void GraphicsFileView::notify_middle_clicked(const QModelIndex& index)
{
  emit middle_clicked(index);
}

void GraphicsFileView::notify_context_menu(const QPoint& global_pos, const QModelIndex& index)
{
  emit context_menu_requested(global_pos, index);
}

void GraphicsFileView::select_row(int row, bool clear_others)
{
  if (clear_others) {
    scene_->clearSelection();
  }
  if (row >= 0 && static_cast<std::size_t>(row) < items_.size()) {
    items_[static_cast<std::size_t>(row)]->setSelected(true);
  }
}

void GraphicsFileView::on_model_reset()
{
  rebuild_items();
}

void GraphicsFileView::on_model_data_changed(const QModelIndex& top_left, const QModelIndex& bottom_right,
                                            const QList<int>& roles)
{
  (void)roles;
  if (!top_left.isValid()) {
    return;
  }
  for (int r = top_left.row(); r <= bottom_right.row(); ++r) {
    if (r >= 0 && static_cast<std::size_t>(r) < items_.size()) {
      items_[static_cast<std::size_t>(r)]->update();
    }
  }
}

void GraphicsFileView::on_scene_selection_changed()
{
  if (!suppress_selection_signal_) {
    emit selection_changed();
  }
}

void GraphicsFileView::resizeEvent(QResizeEvent* event)
{
  QGraphicsView::resizeEvent(event);
  layout_items();
}

void GraphicsFileView::mouseDoubleClickEvent(QMouseEvent* event)
{
  const auto idx = index_at(event->pos());
  if (idx.isValid()) {
    emit activated(idx);
    event->accept();
    return;
  }
  QGraphicsView::mouseDoubleClickEvent(event);
}

void GraphicsFileView::mouseReleaseEvent(QMouseEvent* event)
{
  QGraphicsView::mouseReleaseEvent(event);
}

void GraphicsFileView::contextMenuEvent(QContextMenuEvent* event)
{
  const auto idx = index_at(event->pos());
  emit context_menu_requested(event->globalPos(), idx);
  event->accept();
}

void GraphicsFileView::wheelEvent(QWheelEvent* event)
{
  QGraphicsView::wheelEvent(event);
}

void GraphicsFileView::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::MiddleButton) {
    const auto idx = index_at(event->pos());
    if (idx.isValid()) {
      emit middle_clicked(idx);
      event->accept();
      return;
    }
  }
  if (event->button() == Qt::LeftButton) {
    drag_start_pos_ = event->pos();
    drag_started_ = false;
  }
  QGraphicsView::mousePressEvent(event);
}

void GraphicsFileView::mouseMoveEvent(QMouseEvent* event)
{
  if ((event->buttons() & Qt::LeftButton) && !drag_started_
      && (event->pos() - drag_start_pos_).manhattanLength()
             >= QApplication::startDragDistance()) {
    if (auto* item = qgraphicsitem_cast<GraphicsFileItem*>(itemAt(drag_start_pos_))) {
      if (!item->isSelected()) {
        select_row(item->row(), true);
      }
      start_drag();
      drag_started_ = true;
      event->accept();
      return;
    }
  }
  QGraphicsView::mouseMoveEvent(event);
}

void GraphicsFileView::start_drag()
{
  if (model_ == nullptr) {
    return;
  }
  QModelIndexList indexes;
  for (int row : selected_rows()) {
    indexes.append(model_->index(row, 0));
  }
  if (indexes.isEmpty()) {
    return;
  }
  QMimeData* mime = model_->mimeData(indexes);
  if (mime == nullptr) {
    return;
  }
  begin_drag_action_overlay();
  auto* drag = new QDrag(this);
  drag->setMimeData(mime);
  // Themed cursors (same assets as list/tree views)
  {
    auto load = [](const char* alias) {
      return QPixmap(QStringLiteral(":/icons/%1").arg(QLatin1String(alias)));
    };
    const QPixmap copy = load("dnd-copy.png");
    const QPixmap move = load("dnd-move.png");
    const QPixmap link = load("dnd-link.png");
    const QPixmap none = load("dnd-none.png");
    if (!copy.isNull()) {
      drag->setDragCursor(copy, Qt::CopyAction);
    }
    if (!move.isNull()) {
      drag->setDragCursor(move, Qt::MoveAction);
    }
    if (!link.isNull()) {
      drag->setDragCursor(link, Qt::LinkAction);
    }
    if (!none.isNull()) {
      drag->setDragCursor(none, Qt::IgnoreAction);
    }
  }
  {
    const QIcon icon = indexes.first().data(Qt::DecorationRole).value<QIcon>();
    const QPixmap pm = icon.pixmap(QSize(64, 64));
    if (!pm.isNull()) {
      drag->setPixmap(pm);
      drag->setHotSpot(QPoint(pm.width() / 2, pm.height() / 2));
    }
  }
  // Prefer Move when Shift is held; otherwise Copy (desktop-common). Ctrl also Copy.
  const auto mods = QApplication::keyboardModifiers();
  const Qt::DropAction default_action =
      (mods & Qt::ShiftModifier) ? Qt::MoveAction : Qt::CopyAction;
  drag->exec(Qt::CopyAction | Qt::MoveAction, default_action);
}

void GraphicsFileView::dragEnterEvent(QDragEnterEvent* event)
{
  if (event->mimeData() != nullptr && event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  } else {
    event->ignore();
  }
}

void GraphicsFileView::dragMoveEvent(QDragMoveEvent* event)
{
  if (event->mimeData() != nullptr && event->mimeData()->hasUrls()) {
    // Honour modifier keys for the proposed action while hovering.
    if (event->keyboardModifiers() & Qt::ShiftModifier) {
      event->setDropAction(Qt::MoveAction);
    } else {
      event->setDropAction(Qt::CopyAction);
    }
    event->accept();
  } else {
    event->ignore();
  }
}

void GraphicsFileView::dropEvent(QDropEvent* event)
{
  if (event->mimeData() == nullptr || !event->mimeData()->hasUrls()) {
    event->ignore();
    return;
  }
  QString dest_dir;
  if (auto* item = qgraphicsitem_cast<GraphicsFileItem*>(itemAt(event->position().toPoint()))) {
    if (model_ != nullptr) {
      if (const auto* fi = model_->file_at(item->row()); fi != nullptr && fi->is_directory()) {
        dest_dir = QString::fromStdString(fi->path().string());
      }
    }
  }
  Qt::DropAction action = event->proposedAction();
  if (event->keyboardModifiers() & Qt::ShiftModifier) {
    action = Qt::MoveAction;
  } else if (event->keyboardModifiers() & Qt::ControlModifier) {
    action = Qt::CopyAction;
  }
  emit files_dropped(event->mimeData()->urls(), action, dest_dir);
  event->setDropAction(action);
  event->accept();
}

} // namespace dirtoo::app

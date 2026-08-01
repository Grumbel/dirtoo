// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "graphics_file_view.hpp"
#include "badge_icons.hpp"

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
#include <QDragLeaveEvent>
#include <QMimeData>
#include <QDrag>
#include <QTimer>
#include <QEvent>

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
  if (verticalScrollBar() != nullptr) {
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
      update_visible_window();
    });
  }
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
    if (item != nullptr) {
      item->set_tile_size(tile_size_);
    }
  }
  compute_layout_slots();
  update_visible_window();
}

void GraphicsFileView::set_compact(bool compact)
{
  if (compact_ == compact) {
    return;
  }
  compact_ = compact;
  spacing_ = compact ? 6 : 12;
  compute_layout_slots();
  update_visible_window();
}

void GraphicsFileView::relayout()
{
  compute_layout_slots();
  update_visible_window();
}

void GraphicsFileView::sync_from_model()
{
  rebuild_items();
}

void GraphicsFileView::clear_all_items()
{
  suppress_selection_signal_ = true;
  for (auto* item : items_) {
    if (item != nullptr) {
      scene_->removeItem(item);
      delete item;
    }
  }
  items_.clear();
  slot_pos_.clear();
  suppress_selection_signal_ = false;
}

int GraphicsFileView::cell_width() const
{
  return tile_size_.width() + spacing_;
}

int GraphicsFileView::cell_height() const
{
  return tile_size_.height() + spacing_;
}

int GraphicsFileView::column_count() const
{
  // Match dirtoo-py TileLayout._calc_num_columns:
  // (viewport - 2*padding + spacing) / (tile + spacing)
  const int vp_w = std::max(1, viewport()->width());
  const int cell_w = cell_width(); // tile + spacing
  int cols = std::max(1, (vp_w - 2 * padding_ + spacing_) / std::max(1, cell_w));
  if (compact_) {
    cols = std::max(1, (vp_w - 2 * padding_ + spacing_) / std::max(cell_w, 160));
  }
  return cols;
}

void GraphicsFileView::compute_layout_slots()
{
  const int rows = model_ != nullptr ? model_->rowCount() : 0;
  slot_pos_.assign(static_cast<std::size_t>(rows), QPointF());
  if (rows == 0) {
    layout_cols_ = 1;
    layout_max_row_ = 0;
    scene_->setSceneRect(QRectF());
    return;
  }

  const int cols = column_count();
  const int cell_w = cell_width();
  const int cell_h = cell_height();
  layout_cols_ = cols;

  // Grid content width (tiles left-aligned inside the grid); center the grid
  // in the viewport so left/right padding match (dirtoo-py center_x_off).
  const int grid_w = cols * cell_w - spacing_ + 2 * padding_;
  const int vp_w = std::max(grid_w, viewport()->width());
  const int center_x_off = std::max(0, (vp_w - grid_w) / 2);

  int col = 0;
  int grid_row = 0;
  int max_row = 0;
  for (int i = 0; i < rows; ++i) {
    if (model_ != nullptr) {
      const QModelIndex idx = model_->index(i, 0);
      if (idx.data(IsGroupStartRole).toBool() && i > 0 && col != 0) {
        col = 0;
        ++grid_row;
      }
    }
    slot_pos_[static_cast<std::size_t>(i)] =
        QPointF(center_x_off + padding_ + col * cell_w,
                padding_ + grid_row * cell_h);
    max_row = std::max(max_row, grid_row);
    ++col;
    if (col >= cols) {
      col = 0;
      ++grid_row;
    }
  }
  layout_max_row_ = max_row;
  scene_->setSceneRect(0, 0, vp_w, padding_ * 2 + (max_row + 1) * cell_h);
}

void GraphicsFileView::update_visible_window()
{
  if (model_ == nullptr || slot_pos_.empty()) {
    return;
  }

  const int rows = static_cast<int>(slot_pos_.size());
  if (static_cast<int>(items_.size()) != rows) {
    // Resize sparse vector; drop any items that would be orphaned.
    for (std::size_t i = rows; i < items_.size(); ++i) {
      if (items_[i] != nullptr) {
        scene_->removeItem(items_[i]);
        delete items_[i];
      }
    }
    items_.resize(static_cast<std::size_t>(rows), nullptr);
  }

  const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
  const qreal margin = cell_height() * 3.0; // a few rows above/below
  const qreal top = vis.top() - margin;
  const qreal bottom = vis.bottom() + margin;

  // Binary-search first slot with y + cell_h >= top (non-decreasing y).
  int lo = 0;
  int hi = rows;
  while (lo < hi) {
    const int mid = lo + (hi - lo) / 2;
    if (slot_pos_[static_cast<std::size_t>(mid)].y() + cell_height() < top) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  const int first = lo;
  lo = first;
  hi = rows;
  while (lo < hi) {
    const int mid = lo + (hi - lo) / 2;
    if (slot_pos_[static_cast<std::size_t>(mid)].y() <= bottom) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  const int last = lo; // exclusive

  suppress_selection_signal_ = true;
  // Snapshot selection from live items into the persistent set.
  for (auto* item : items_) {
    if (item != nullptr && item->isSelected()) {
      selected_row_set_.insert(item->row());
    }
  }
  for (int r = 0; r < rows; ++r) {
    const bool want = r >= first && r < last;
    auto*& item = items_[static_cast<std::size_t>(r)];
    if (!want) {
      if (item != nullptr) {
        if (item->isSelected()) {
          selected_row_set_.insert(r);
        }
        scene_->removeItem(item);
        delete item;
        item = nullptr;
      }
      continue;
    }
    if (item == nullptr) {
      item = new GraphicsFileItem(model_, r, this);
      item->set_tile_size(tile_size_);
      scene_->addItem(item);
      if (selected_row_set_.contains(r)) {
        item->setSelected(true);
      }
    } else {
      item->set_row(r);
      item->set_tile_size(tile_size_);
      item->setSelected(selected_row_set_.contains(r));
    }
    item->setPos(slot_pos_[static_cast<std::size_t>(r)]);
  }
  suppress_selection_signal_ = false;
}

void GraphicsFileView::rebuild_items()
{
  clear_all_items();
  selected_row_set_.clear();
  if (model_ == nullptr) {
    scene_->setSceneRect(QRectF());
    return;
  }
  const int rows = model_->rowCount();
  items_.assign(static_cast<std::size_t>(rows), nullptr);
  compute_layout_slots();
  update_visible_window();
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
  QSet<int> set = selected_row_set_;
  for (auto* item : items_) {
    if (item != nullptr && item->isSelected()) {
      set.insert(item->row());
    } else if (item != nullptr && !item->isSelected()) {
      set.remove(item->row());
    }
  }
  std::vector<int> rows;
  rows.reserve(static_cast<std::size_t>(set.size()));
  for (int r : set) {
    rows.push_back(r);
  }
  std::sort(rows.begin(), rows.end());
  return rows;
}

void GraphicsFileView::clear_selection()
{
  selected_row_set_.clear();
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

void GraphicsFileView::select_all()
{
  selected_row_set_.clear();
  const int rows = model_ != nullptr ? model_->rowCount() : 0;
  for (int r = 0; r < rows; ++r) {
    selected_row_set_.insert(r);
  }
  suppress_selection_signal_ = true;
  for (auto* item : items_) {
    if (item != nullptr) {
      item->setSelected(true);
    }
  }
  suppress_selection_signal_ = false;
  emit selection_changed();
}

void GraphicsFileView::select_row(int row, bool clear_others)
{
  if (clear_others) {
    selected_row_set_.clear();
    scene_->clearSelection();
  }
  if (row < 0 || static_cast<std::size_t>(row) >= items_.size()) {
    return;
  }
  // Ensure the target is materialized (may be outside the current window).
  if (items_[static_cast<std::size_t>(row)] == nullptr && model_ != nullptr
      && static_cast<std::size_t>(row) < slot_pos_.size()) {
    auto* item = new GraphicsFileItem(model_, row, this);
    item->set_tile_size(tile_size_);
    item->setPos(slot_pos_[static_cast<std::size_t>(row)]);
    scene_->addItem(item);
    items_[static_cast<std::size_t>(row)] = item;
  }
  selected_row_set_.insert(row);
  if (items_[static_cast<std::size_t>(row)] != nullptr) {
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
    if (r >= 0 && static_cast<std::size_t>(r) < items_.size()
        && items_[static_cast<std::size_t>(r)] != nullptr) {
      items_[static_cast<std::size_t>(r)]->update();
    }
  }
}

void GraphicsFileView::on_scene_selection_changed()
{
  if (suppress_selection_signal_) {
    return;
  }
  // Merge live item selection with rows that remain selected off-window.
  QSet<int> live_selected;
  QSet<int> live_rows;
  for (auto* item : items_) {
    if (item == nullptr) {
      continue;
    }
    live_rows.insert(item->row());
    if (item->isSelected()) {
      live_selected.insert(item->row());
    }
  }
  // Drop rows that are live but no longer selected; keep off-window selections.
  for (int r : live_rows) {
    if (live_selected.contains(r)) {
      selected_row_set_.insert(r);
    } else {
      selected_row_set_.remove(r);
    }
  }
  emit selection_changed();
}

void GraphicsFileView::resizeEvent(QResizeEvent* event)
{
  QGraphicsView::resizeEvent(event);
  compute_layout_slots();
  update_visible_window();
}

void GraphicsFileView::scrollContentsBy(int dx, int dy)
{
  QGraphicsView::scrollContentsBy(dx, dy);
  update_visible_window();
}

void GraphicsFileView::changeEvent(QEvent* event)
{
  QGraphicsView::changeEvent(event);
  if (event->type() == QEvent::PaletteChange) {
    setBackgroundBrush(palette().base());
    // Captions/selection colors are palette-driven in item paint; refresh live tiles.
    for (auto* item : items_) {
      if (item != nullptr) {
        item->update();
      }
    }
  }
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
  // Restore rubber-band as the default empty-area interaction.
  setDragMode(QGraphicsView::RubberBandDrag);
}

void GraphicsFileView::contextMenuEvent(QContextMenuEvent* event)
{
  const auto idx = index_at(event->pos());
  // Keep multi-selection when right-clicking an already-selected tile.
  if (idx.isValid() && !selected_row_set_.contains(idx.row())) {
    select_row(idx.row(), true);
  }
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
  if (event->button() == Qt::RightButton) {
    // Avoid QGraphicsView clearing multi-selection on right-click.
    // Selection is adjusted (if needed) in contextMenuEvent.
    event->accept();
    return;
  }
  if (event->button() == Qt::LeftButton) {
    drag_start_pos_ = event->pos();
    drag_started_ = false;
    // Item press → item drag (no rubber-band). Empty background → rubber-band select.
    if (qgraphicsitem_cast<GraphicsFileItem*>(itemAt(event->pos())) != nullptr) {
      setDragMode(QGraphicsView::NoDrag);
    } else {
      setDragMode(QGraphicsView::RubberBandDrag);
    }
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
      return load_badge_pixmap(QString::fromLatin1(alias));
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
  // Desktop norms: Shift=Move, Ctrl=Copy, Alt or Ctrl+Shift=Link.
  const auto mods = QApplication::keyboardModifiers();
  Qt::DropAction default_action = Qt::CopyAction;
  if ((mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier)) {
    default_action = Qt::LinkAction;
  } else if (mods & Qt::AltModifier) {
    default_action = Qt::LinkAction;
  } else if (mods & Qt::ShiftModifier) {
    default_action = Qt::MoveAction;
  } else if (mods & Qt::ControlModifier) {
    default_action = Qt::CopyAction;
  }
  drag->exec(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, default_action);
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
    const auto mods = event->modifiers();
    if ((mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier)) {
      event->setDropAction(Qt::LinkAction);
    } else if (mods & Qt::AltModifier) {
      event->setDropAction(Qt::LinkAction);
    } else if (mods & Qt::ShiftModifier) {
      event->setDropAction(Qt::MoveAction);
    } else if (mods & Qt::ControlModifier) {
      event->setDropAction(Qt::CopyAction);
    } else {
      // Prefer proposed action when the source offers Move (same-app default).
      const Qt::DropAction proposed = event->proposedAction();
      if (proposed == Qt::MoveAction || proposed == Qt::CopyAction || proposed == Qt::LinkAction) {
        event->setDropAction(proposed);
      } else {
        event->setDropAction(Qt::CopyAction);
      }
    }
    // itemAt expects viewport coordinates; event position is view-local.
    const QPoint vp = viewport()->mapFrom(this, event->position().toPoint());
    GraphicsFileItem* under = qgraphicsitem_cast<GraphicsFileItem*>(itemAt(vp));
    bool under_is_dir = false;
    if (under != nullptr && model_ != nullptr) {
      if (const auto* fi = model_->file_at(under->row()); fi != nullptr && fi->is_directory()) {
        under_is_dir = true;
      }
    }
    for (GraphicsFileItem* it : items_) {
      if (it == nullptr) {
        continue;
      }
      it->set_drop_target(under_is_dir && it == under);
    }
    event->acceptProposedAction();
  } else {
    event->ignore();
  }
}

void GraphicsFileView::dragLeaveEvent(QDragLeaveEvent* event)
{
  for (GraphicsFileItem* it : items_) {
    if (it != nullptr) {
      it->set_drop_target(false);
    }
  }
  QGraphicsView::dragLeaveEvent(event);
}

void GraphicsFileView::dropEvent(QDropEvent* event)
{
  for (GraphicsFileItem* it : items_) {
    if (it != nullptr) {
      it->set_drop_target(false);
    }
  }
  if (event->mimeData() == nullptr || !event->mimeData()->hasUrls()) {
    event->ignore();
    return;
  }
  // Viewport coordinates for item lookup (same as dragMoveEvent).
  const QPoint vp = viewport()->mapFrom(this, event->position().toPoint());
  QString dest_dir;
  if (auto* item = qgraphicsitem_cast<GraphicsFileItem*>(itemAt(vp))) {
    if (model_ != nullptr) {
      if (const auto* fi = model_->file_at(item->row()); fi != nullptr && fi->is_directory()) {
        dest_dir = QString::fromStdString(fi->path().string());
      }
    }
  }
  const auto mods = event->modifiers();
  Qt::DropAction action = event->proposedAction();
  if ((mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier)) {
    action = Qt::LinkAction;
  } else if (mods & Qt::AltModifier) {
    action = Qt::LinkAction;
  } else if (mods & Qt::ShiftModifier) {
    action = Qt::MoveAction;
  } else if (mods & Qt::ControlModifier) {
    action = Qt::CopyAction;
  } else if (action != Qt::CopyAction && action != Qt::MoveAction && action != Qt::LinkAction) {
    action = Qt::CopyAction;
  }
  emit files_dropped(event->mimeData()->urls(), action, dest_dir);
  event->setDropAction(action);
  event->acceptProposedAction();
}

} // namespace dirtoo::app

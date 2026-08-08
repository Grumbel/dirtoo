// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "graphics_file_view.hpp"
#include "badge_icons.hpp"

#include "file_list_model.hpp"
#include "graphics_file_item.hpp"
#include "drag_action_overlay.hpp"

#include <QContextMenuEvent>
#include <QFocusEvent>
#include <QGraphicsScene>
#include <QKeyEvent>
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
  set_cursor_row(row, false);
}

void GraphicsFileView::update_cursor_item_visuals(int old_row, int new_row)
{
  auto touch = [this](int r) {
    if (r < 0 || static_cast<std::size_t>(r) >= items_.size()) {
      return;
    }
    if (items_[static_cast<std::size_t>(r)] != nullptr) {
      items_[static_cast<std::size_t>(r)]->update();
    }
  };
  touch(old_row);
  touch(new_row);
}

bool GraphicsFileView::is_row_on_screen(int row) const
{
  if (row < 0 || static_cast<std::size_t>(row) >= slot_pos_.size()) {
    return false;
  }
  const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
  const QPointF p = slot_pos_[static_cast<std::size_t>(row)];
  const QRectF tile(p.x(), p.y(), tile_size_.width(), tile_size_.height());
  // Require a meaningful intersection so barely-clipped tiles count as off-screen.
  return vis.intersects(tile.adjusted(4, 4, -4, -4));
}

int GraphicsFileView::warp_cursor_to_visible()
{
  const int rows = model_ != nullptr ? model_->rowCount() : 0;
  if (rows <= 0 || slot_pos_.empty()) {
    return -1;
  }
  const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
  int best = -1;
  qreal best_dist = 1e18;
  // Prefer a fully contained tile; else nearest intersecting; else nearest by y.
  for (int i = 0; i < rows; ++i) {
    const QPointF p = slot_pos_[static_cast<std::size_t>(i)];
    const QRectF tile(p.x(), p.y(), tile_size_.width(), tile_size_.height());
    if (vis.contains(tile)) {
      const qreal d = qAbs(p.y() - vis.center().y());
      if (d < best_dist) {
        best_dist = d;
        best = i;
      }
    }
  }
  if (best < 0) {
    for (int i = 0; i < rows; ++i) {
      const QPointF p = slot_pos_[static_cast<std::size_t>(i)];
      const QRectF tile(p.x(), p.y(), tile_size_.width(), tile_size_.height());
      if (vis.intersects(tile)) {
        const qreal d = qAbs(p.y() - vis.center().y());
        if (d < best_dist) {
          best_dist = d;
          best = i;
        }
      }
    }
  }
  if (best < 0) {
    // Extreme: pick closest slot to viewport center without scrolling.
    const QPointF c = vis.center();
    for (int i = 0; i < rows; ++i) {
      const QPointF p = slot_pos_[static_cast<std::size_t>(i)];
      const qreal d = (p.x() - c.x()) * (p.x() - c.x()) + (p.y() - c.y()) * (p.y() - c.y());
      if (d < best_dist) {
        best_dist = d;
        best = i;
      }
    }
  }
  return best;
}

void GraphicsFileView::set_cursor_row(int row, bool ensure_visible)
{
  const int rows = model_ != nullptr ? model_->rowCount() : 0;
  if (row < 0 || row >= rows) {
    const int old = cursor_row_;
    cursor_row_ = -1;
    update_cursor_item_visuals(old, -1);
    return;
  }

  const int old = cursor_row_;
  const bool old_on_screen = (old >= 0) && is_row_on_screen(old);

  if (cursor_row_ == row) {
    if (ensure_visible && !is_row_on_screen(row)) {
      if (old_on_screen) {
        ensure_cursor_visible(); // follow: scroll
      } else {
        const int warped = warp_cursor_to_visible();
        if (warped >= 0 && warped != row) {
          set_cursor_row(warped, false);
        }
      }
    }
    return;
  }

  cursor_row_ = row;
  // Materialize if needed so the outline paints.
  if (static_cast<std::size_t>(row) < items_.size() && items_[static_cast<std::size_t>(row)] == nullptr
      && static_cast<std::size_t>(row) < slot_pos_.size()) {
    auto* item = new GraphicsFileItem(model_, row, this);
    item->set_tile_size(tile_size_);
    item->setPos(slot_pos_[static_cast<std::size_t>(row)]);
    if (selected_row_set_.contains(row)) {
      item->setSelected(true);
    }
    scene_->addItem(item);
    items_[static_cast<std::size_t>(row)] = item;
  }
  update_cursor_item_visuals(old, row);

  if (!ensure_visible) {
    return;
  }

  // Policy (TODO): do not yank the viewport to an off-screen cursor. Warp the
  // cursor onto the visible area instead. Only scroll when the previous cursor
  // was on-screen and this move would leave the viewport (follow mode).
  if (is_row_on_screen(row)) {
    return;
  }
  if (old_on_screen) {
    ensure_cursor_visible();
    return;
  }
  const int warped = warp_cursor_to_visible();
  if (warped >= 0 && warped != cursor_row_) {
    // Re-enter without ensure_visible to avoid recursion loops.
    const int prev = cursor_row_;
    cursor_row_ = warped;
    if (static_cast<std::size_t>(warped) < items_.size()
        && items_[static_cast<std::size_t>(warped)] == nullptr
        && static_cast<std::size_t>(warped) < slot_pos_.size()) {
      auto* item = new GraphicsFileItem(model_, warped, this);
      item->set_tile_size(tile_size_);
      item->setPos(slot_pos_[static_cast<std::size_t>(warped)]);
      if (selected_row_set_.contains(warped)) {
        item->setSelected(true);
      }
      scene_->addItem(item);
      items_[static_cast<std::size_t>(warped)] = item;
    }
    update_cursor_item_visuals(prev, warped);
  }
}

void GraphicsFileView::clear_cursor()
{
  set_cursor_row(-1, false);
}

void GraphicsFileView::ensure_cursor_visible()
{
  if (cursor_row_ < 0 || static_cast<std::size_t>(cursor_row_) >= slot_pos_.size()) {
    return;
  }
  const QPointF p = slot_pos_[static_cast<std::size_t>(cursor_row_)];
  const QRectF r(p.x(), p.y(), tile_size_.width(), tile_size_.height());
  ensureVisible(r, 8, 8);
  update_visible_window();
}

void GraphicsFileView::cursor_move(int dx, int dy)
{
  const int rows = model_ != nullptr ? model_->rowCount() : 0;
  if (rows <= 0 || slot_pos_.empty()) {
    return;
  }

  if (cursor_row_ < 0 || cursor_row_ >= rows) {
    // Seed cursor: first fully visible tile, else row 0 (Python best_item).
    const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
    int best = 0;
    for (int i = 0; i < rows; ++i) {
      const QPointF p = slot_pos_[static_cast<std::size_t>(i)];
      const QRectF tile(p.x(), p.y(), tile_size_.width(), tile_size_.height());
      if (vis.contains(tile)) {
        best = i;
        break;
      }
      if (vis.intersects(tile)) {
        best = i;
        break;
      }
    }
    set_cursor_row(best, true);
    return;
  }

  const QPointF cur = slot_pos_[static_cast<std::size_t>(cursor_row_)];
  const qreal step_x = cell_width();
  const qreal step_y = cell_height();
  const QRectF probe(cur.x() + step_x * dx - 2, cur.y() + step_y * dy - 2,
                     tile_size_.width() + 4, tile_size_.height() + 4);

  int best = -1;
  qreal best_dist = 1e18;
  for (int i = 0; i < rows; ++i) {
    if (i == cursor_row_) {
      continue;
    }
    const QPointF p = slot_pos_[static_cast<std::size_t>(i)];
    const QRectF tile(p.x(), p.y(), tile_size_.width(), tile_size_.height());
    if (!probe.intersects(tile) && !probe.contains(p)) {
      // Also accept nearest in the intended direction if probe misses (group breaks).
      continue;
    }
    const qreal d = (p.x() - cur.x()) * (p.x() - cur.x()) + (p.y() - cur.y()) * (p.y() - cur.y());
    if (d < best_dist) {
      best_dist = d;
      best = i;
    }
  }
  if (best < 0) {
    // Fallback: step by column count when pure geometric probe finds nothing.
    const int cols = std::max(1, layout_cols_);
    int next = cursor_row_;
    if (dx < 0) {
      next = std::max(0, cursor_row_ - 1);
    } else if (dx > 0) {
      next = std::min(rows - 1, cursor_row_ + 1);
    } else if (dy < 0) {
      next = std::max(0, cursor_row_ - cols);
    } else if (dy > 0) {
      next = std::min(rows - 1, cursor_row_ + cols);
    }
    if (next != cursor_row_) {
      best = next;
    }
  }
  if (best >= 0) {
    set_cursor_row(best, true);
  }
}

void GraphicsFileView::keyPressEvent(QKeyEvent* event)
{
  if (event == nullptr) {
    return;
  }
  const auto mods = event->modifiers();
  const bool shift = mods & Qt::ShiftModifier;
  const bool ctrl = mods & Qt::ControlModifier;

  auto extend_select_current = [this] {
    if (cursor_row_ >= 0) {
      selected_row_set_.insert(cursor_row_);
      if (static_cast<std::size_t>(cursor_row_) < items_.size()
          && items_[static_cast<std::size_t>(cursor_row_)] != nullptr) {
        items_[static_cast<std::size_t>(cursor_row_)]->setSelected(true);
      }
      emit selection_changed();
    }
  };

  switch (event->key()) {
  case Qt::Key_Escape:
    clear_selection();
    clear_cursor();
    event->accept();
    return;
  case Qt::Key_Left:
    if (shift) {
      extend_select_current();
    }
    cursor_move(-1, 0);
    event->accept();
    return;
  case Qt::Key_Right:
    if (shift) {
      extend_select_current();
    }
    cursor_move(1, 0);
    event->accept();
    return;
  case Qt::Key_Up:
    if (shift) {
      extend_select_current();
    }
    cursor_move(0, -1);
    event->accept();
    return;
  case Qt::Key_Down:
    if (shift) {
      extend_select_current();
    }
    cursor_move(0, 1);
    event->accept();
    return;
  case Qt::Key_Space:
    // Toggle selection on cursor item: Ctrl+Space (Python) or Shift+Space.
    if ((ctrl || shift) && cursor_row_ >= 0) {
      if (selected_row_set_.contains(cursor_row_)) {
        selected_row_set_.remove(cursor_row_);
        if (static_cast<std::size_t>(cursor_row_) < items_.size()
            && items_[static_cast<std::size_t>(cursor_row_)] != nullptr) {
          items_[static_cast<std::size_t>(cursor_row_)]->setSelected(false);
        }
      } else {
        selected_row_set_.insert(cursor_row_);
        if (static_cast<std::size_t>(cursor_row_) < items_.size()
            && items_[static_cast<std::size_t>(cursor_row_)] != nullptr) {
          items_[static_cast<std::size_t>(cursor_row_)]->setSelected(true);
        }
      }
      emit selection_changed();
      event->accept();
      return;
    }
    break;
  case Qt::Key_Return:
  case Qt::Key_Enter:
    if (cursor_row_ >= 0 && model_ != nullptr) {
      const QModelIndex idx = model_->index(cursor_row_, 0);
      if (idx.isValid()) {
        emit activated(idx);
        event->accept();
        return;
      }
    }
    break;
  default:
    break;
  }
  QGraphicsView::keyPressEvent(event);
}

void GraphicsFileView::focusInEvent(QFocusEvent* event)
{
  QGraphicsView::focusInEvent(event);
  if (cursor_row_ < 0 && model_ != nullptr && model_->rowCount() > 0) {
    // Soft seed without scrolling when focus arrives empty.
    cursor_move(0, 0);
  }
}

} // namespace dirtoo::app

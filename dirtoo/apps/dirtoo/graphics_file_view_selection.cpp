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

void GraphicsFileView::materialize_row(int row)
{
  if (row < 0 || static_cast<std::size_t>(row) >= items_.size()
      || static_cast<std::size_t>(row) >= slot_pos_.size()) {
    return;
  }
  if (items_[static_cast<std::size_t>(row)] != nullptr) {
    return;
  }
  auto* item = new GraphicsFileItem(model_, row, this);
  const QSize tile =
      (static_cast<std::size_t>(row) < slot_tile_size_.size())
          ? slot_tile_size_[static_cast<std::size_t>(row)]
          : tile_size_;
  item->set_tile_size(tile);
  item->setPos(slot_pos_[static_cast<std::size_t>(row)]);
  if (selected_row_set_.contains(row)) {
    item->setSelected(true);
  }
  scene_->addItem(item);
  items_[static_cast<std::size_t>(row)] = item;
}

void GraphicsFileView::clear_selection()
{
  suppress_selection_signal_ = true;
  selected_row_set_.clear();
  scene_->clearSelection();
  suppress_selection_signal_ = false;
  selection_anchor_row_ = -1;
  pending_single_select_row_ = -1;
  emit selection_changed();
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

void GraphicsFileView::notify_tag_chip_clicked(const QString& tag_name)
{
  if (!tag_name.isEmpty()) {
    emit tag_chip_clicked(tag_name);
  }
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
    suppress_selection_signal_ = true;
    selected_row_set_.clear();
    scene_->clearSelection();
    suppress_selection_signal_ = false;
  }
  if (row < 0 || model_ == nullptr || row >= model_->rowCount()) {
    if (clear_others) {
      emit selection_changed();
    }
    return;
  }
  // Ensure the target is materialized (may be outside the current window).
  materialize_row(row);
  selected_row_set_.insert(row);
  if (static_cast<std::size_t>(row) < items_.size() && items_[static_cast<std::size_t>(row)] != nullptr) {
    suppress_selection_signal_ = true;
    items_[static_cast<std::size_t>(row)]->setSelected(true);
    suppress_selection_signal_ = false;
  }
  selection_anchor_row_ = row;
  set_cursor_row(row, false);
  emit selection_changed();
}

void GraphicsFileView::select_range(int from_row, int to_row, bool clear_others)
{
  if (model_ == nullptr) {
    return;
  }
  const int rows = model_->rowCount();
  if (rows <= 0) {
    return;
  }
  from_row = std::clamp(from_row, 0, rows - 1);
  to_row = std::clamp(to_row, 0, rows - 1);
  if (from_row > to_row) {
    std::swap(from_row, to_row);
  }
  if (clear_others) {
    suppress_selection_signal_ = true;
    selected_row_set_.clear();
    scene_->clearSelection();
    suppress_selection_signal_ = false;
  }
  suppress_selection_signal_ = true;
  for (int r = from_row; r <= to_row; ++r) {
    selected_row_set_.insert(r);
    materialize_row(r);
    if (static_cast<std::size_t>(r) < items_.size() && items_[static_cast<std::size_t>(r)] != nullptr) {
      items_[static_cast<std::size_t>(r)]->setSelected(true);
    }
  }
  suppress_selection_signal_ = false;
  emit selection_changed();
}

void GraphicsFileView::apply_click_selection(int row, Qt::KeyboardModifiers mods)
{
  if (row < 0 || model_ == nullptr || row >= model_->rowCount()) {
    return;
  }
  const bool ctrl = mods & Qt::ControlModifier;
  const bool shift = mods & Qt::ShiftModifier;

  if (shift) {
    const int anchor = (selection_anchor_row_ >= 0) ? selection_anchor_row_ : row;
    select_range(anchor, row, /*clear_others=*/!ctrl);
    set_cursor_row(row, false);
    return;
  }
  if (ctrl) {
    const bool now = !selected_row_set_.contains(row);
    set_row_selected(row, now);
    selection_anchor_row_ = row;
    set_cursor_row(row, false);
    return;
  }
  // Plain click: select only this row (caller handles deferred case for
  // already-selected tiles).
  select_row(row, /*clear_others=*/true);
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
  const QSize tile_sz =
      (static_cast<std::size_t>(row) < slot_tile_size_.size())
          ? slot_tile_size_[static_cast<std::size_t>(row)]
          : tile_size_;
  const QRectF tile(p.x(), p.y(), tile_sz.width(), tile_sz.height());
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
    const QSize tile_sz =
        (static_cast<std::size_t>(i) < slot_tile_size_.size())
            ? slot_tile_size_[static_cast<std::size_t>(i)]
            : tile_size_;
    const QRectF tile(p.x(), p.y(), tile_sz.width(), tile_sz.height());
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
      const QSize tile_sz =
          (static_cast<std::size_t>(i) < slot_tile_size_.size())
              ? slot_tile_size_[static_cast<std::size_t>(i)]
              : tile_size_;
      const QRectF tile(p.x(), p.y(), tile_sz.width(), tile_sz.height());
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
  materialize_row(row);
  update_cursor_item_visuals(old, row);

  if (!ensure_visible) {
    return;
  }

  // Policy: do not yank the viewport to an off-screen cursor. Warp the
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
    materialize_row(warped);
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
  const QSize tile_sz =
      (static_cast<std::size_t>(cursor_row_) < slot_tile_size_.size())
          ? slot_tile_size_[static_cast<std::size_t>(cursor_row_)]
          : tile_size_;
  const QRectF r(p.x(), p.y(), tile_sz.width(), tile_sz.height());
  ensureVisible(r, 8, 8);
  update_visible_window();
}

void GraphicsFileView::cursor_move(int dx, int dy)
{
  const int rows = model_ != nullptr ? model_->rowCount() : 0;
  if (rows <= 0 || slot_pos_.empty()) {
    return;
  }

  // Zero movement only seeds a missing cursor; never jumps to a neighbour.
  if (dx == 0 && dy == 0) {
    if (cursor_row_ >= 0 && cursor_row_ < rows) {
      return;
    }
  }

  if (cursor_row_ < 0 || cursor_row_ >= rows) {
    // Seed cursor: first fully visible tile, else first intersecting, else 0.
    const QRectF vis = mapToScene(viewport()->rect()).boundingRect();
    int best = 0;
    for (int i = 0; i < rows; ++i) {
      const QPointF p = slot_pos_[static_cast<std::size_t>(i)];
      const QSize tile_sz =
          (static_cast<std::size_t>(i) < slot_tile_size_.size())
              ? slot_tile_size_[static_cast<std::size_t>(i)]
              : tile_size_;
      const QRectF tile(p.x(), p.y(), tile_sz.width(), tile_sz.height());
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

  if (dx == 0 && dy == 0) {
    return;
  }

  const QPointF cur = slot_pos_[static_cast<std::size_t>(cursor_row_)];
  const qreal step_x = cell_width();
  const qreal step_y = cell_height();
  const QSize cur_tile =
      (static_cast<std::size_t>(cursor_row_) < slot_tile_size_.size())
          ? slot_tile_size_[static_cast<std::size_t>(cursor_row_)]
          : tile_size_;
  const QRectF probe(cur.x() + step_x * dx - 2, cur.y() + step_y * dy - 2,
                     cur_tile.width() + 4, cur_tile.height() + 4);

  int best = -1;
  qreal best_dist = 1e18;
  for (int i = 0; i < rows; ++i) {
    if (i == cursor_row_) {
      continue;
    }
    const QPointF p = slot_pos_[static_cast<std::size_t>(i)];
    const QSize tile_sz =
        (static_cast<std::size_t>(i) < slot_tile_size_.size())
            ? slot_tile_size_[static_cast<std::size_t>(i)]
            : tile_size_;
    const QRectF tile(p.x(), p.y(), tile_sz.width(), tile_sz.height());
    if (!probe.intersects(tile) && !probe.contains(p)) {
      // Also accept nearest in the intended direction if probe misses (group breaks).
      continue;
    }
    // Prefer items that lie in the intended direction.
    const qreal ddx = p.x() - cur.x();
    const qreal ddy = p.y() - cur.y();
    if (dx < 0 && ddx > 0) {
      continue;
    }
    if (dx > 0 && ddx < 0) {
      continue;
    }
    if (dy < 0 && ddy > 0) {
      continue;
    }
    if (dy > 0 && ddy < 0) {
      continue;
    }
    const qreal d = ddx * ddx + ddy * ddy;
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



void GraphicsFileView::set_row_selected(int row, bool selected)
{
  if (row < 0 || model_ == nullptr || row >= model_->rowCount()) {
    return;
  }
  const bool was = selected_row_set_.contains(row);
  if (selected == was) {
    if (static_cast<std::size_t>(row) < items_.size() && items_[static_cast<std::size_t>(row)] != nullptr) {
      suppress_selection_signal_ = true;
      items_[static_cast<std::size_t>(row)]->setSelected(selected);
      suppress_selection_signal_ = false;
    }
    return;
  }
  if (selected) {
    selected_row_set_.insert(row);
  } else {
    selected_row_set_.remove(row);
  }
  if (static_cast<std::size_t>(row) < items_.size() && items_[static_cast<std::size_t>(row)] != nullptr) {
    suppress_selection_signal_ = true;
    items_[static_cast<std::size_t>(row)]->setSelected(selected);
    suppress_selection_signal_ = false;
  }
  emit selection_changed();
}

void GraphicsFileView::shift_paint_step(int dx, int dy)
{
  if (cursor_row_ < 0 && model_ != nullptr && model_->rowCount() > 0) {
    set_cursor_row(0, true);
  }
  if (cursor_row_ < 0) {
    return;
  }
  if (!shift_paint_active_) {
    // Toggle current file; that new state is painted onto further cells.
    const bool now_selected = !selected_row_set_.contains(cursor_row_);
    set_row_selected(cursor_row_, now_selected);
    shift_paint_select_ = now_selected;
    shift_paint_active_ = true;
    selection_anchor_row_ = cursor_row_;
  } else {
    set_row_selected(cursor_row_, shift_paint_select_);
  }
  const int before = cursor_row_;
  cursor_move(dx, dy);
  if (cursor_row_ >= 0 && cursor_row_ != before) {
    set_row_selected(cursor_row_, shift_paint_select_);
  }
}

void GraphicsFileView::keyPressEvent(QKeyEvent* event)
{
  if (event == nullptr) {
    return;
  }
  const auto mods = event->modifiers();
  const bool shift = mods & Qt::ShiftModifier;

  switch (event->key()) {
  case Qt::Key_Escape:
    // Clear multi-selection; keep the keyboard cursor so navigation continues.
    clear_selection();
    shift_paint_active_ = false;
    event->accept();
    return;
  case Qt::Key_Left:
    if (shift) {
      shift_paint_step(-1, 0);
    } else {
      shift_paint_active_ = false;
      cursor_move(-1, 0);
    }
    event->accept();
    return;
  case Qt::Key_Right:
    if (shift) {
      shift_paint_step(1, 0);
    } else {
      shift_paint_active_ = false;
      cursor_move(1, 0);
    }
    event->accept();
    return;
  case Qt::Key_Up:
    if (shift) {
      shift_paint_step(0, -1);
    } else {
      shift_paint_active_ = false;
      cursor_move(0, -1);
    }
    event->accept();
    return;
  case Qt::Key_Down:
    if (shift) {
      shift_paint_step(0, 1);
    } else {
      shift_paint_active_ = false;
      cursor_move(0, 1);
    }
    event->accept();
    return;
  case Qt::Key_Space:
    // Space toggles the cursor tile (with or without modifiers).
    if (cursor_row_ >= 0) {
      const bool now_selected = !selected_row_set_.contains(cursor_row_);
      set_row_selected(cursor_row_, now_selected);
      // Seed paint mode so the next Shift+arrow keeps this value.
      shift_paint_select_ = now_selected;
      shift_paint_active_ = true;
      selection_anchor_row_ = cursor_row_;
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

void GraphicsFileView::keyReleaseEvent(QKeyEvent* event)
{
  if (event != nullptr && event->key() == Qt::Key_Shift) {
    shift_paint_active_ = false;
  }
  QGraphicsView::keyReleaseEvent(event);
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

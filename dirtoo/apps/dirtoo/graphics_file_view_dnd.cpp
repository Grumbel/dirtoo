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
#include <vector>

namespace dirtoo::app {

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
  // Deferred single-select: press was on an already-selected tile without
  // modifiers, and the user did not start a drag → collapse to that row only.
  if (event->button() == Qt::LeftButton && !drag_started_
      && pending_single_select_row_ >= 0) {
    select_row(pending_single_select_row_, /*clear_others=*/true);
  }
  pending_single_select_row_ = -1;
  press_row_ = -1;
  drag_started_ = false;

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
  pending_single_select_row_ = -1;
  press_row_ = -1;
  drag_started_ = false;

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
    // Item press → item drag (no rubber-band). Empty background → rubber-band select.
    if (auto* gfi = qgraphicsitem_cast<GraphicsFileItem*>(itemAt(event->pos()))) {
      setDragMode(QGraphicsView::NoDrag);
      const int row = gfi->row();
      press_row_ = row;
      set_cursor_row(row, false);

      const auto mods = event->modifiers();
      const bool ctrl = mods & Qt::ControlModifier;
      const bool shift = mods & Qt::ShiftModifier;
      const bool already = selected_row_set_.contains(row);

      if (shift || ctrl) {
        // Modifier clicks: apply immediately; no deferred collapse.
        apply_click_selection(row, mods);
        // Do not forward to QGraphicsItem::mousePressEvent — that would fight
        // our selection logic (Qt clears/toggles based on its own rules).
        event->accept();
        return;
      }

      if (already) {
        // Keep multi-selection for a potential drag. Collapse on release if
        // the user never dragged.
        pending_single_select_row_ = row;
        // Do not call QGraphicsItem selection path — it would clear others.
        event->accept();
        return;
      }

      // Unselected tile, plain click: select only this row now.
      apply_click_selection(row, mods);
      event->accept();
      return;
    }
    // Empty background: allow rubber-band. Clear selection on press unless
    // Ctrl is held (additive rubber-band is uncommon; Qt still manages the
    // band itself).
    setDragMode(QGraphicsView::RubberBandDrag);
    if (!(event->modifiers() & Qt::ControlModifier)) {
      clear_selection();
    }
    pending_single_select_row_ = -1;
  }
  QGraphicsView::mousePressEvent(event);
}

void GraphicsFileView::mouseMoveEvent(QMouseEvent* event)
{
  if ((event->buttons() & Qt::LeftButton) && !drag_started_
      && (event->pos() - drag_start_pos_).manhattanLength()
             >= QApplication::startDragDistance()) {
    // Prefer the row recorded at press (item may have scrolled under the
    // pointer, or the press was accepted without going through itemAt again).
    int row = press_row_;
    if (row < 0) {
      if (auto* item = qgraphicsitem_cast<GraphicsFileItem*>(itemAt(drag_start_pos_))) {
        row = item->row();
      }
    }
    if (row >= 0 && model_ != nullptr && row < model_->rowCount()) {
      const auto mods = QApplication::keyboardModifiers();
      const bool extend = (mods & Qt::ControlModifier) || (mods & Qt::ShiftModifier);
      // Without Ctrl/Shift: drag either the existing multi-selection (if this
      // row is already in it) or only the pressed row. Prevents unrelated
      // residual selection from entering the drag payload.
      if (!extend) {
        if (!selected_row_set_.contains(row)) {
          select_row(row, true);
        }
        // Cancel deferred collapse — we are dragging the multi-selection.
        pending_single_select_row_ = -1;
      } else if (!selected_row_set_.contains(row)) {
        select_row(row, /*clear_others=*/false);
        pending_single_select_row_ = -1;
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
  // Use the authoritative selected_row_set_ only (not the merge with transient
  // QGraphicsItem::isSelected state), so drag payload matches what the user
  // intentionally selected.
  QModelIndexList indexes;
  std::vector<int> rows;
  rows.reserve(static_cast<std::size_t>(selected_row_set_.size()));
  for (int row : selected_row_set_) {
    rows.push_back(row);
  }
  std::sort(rows.begin(), rows.end());
  for (int row : rows) {
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
    drag_entered_ = true;
    event->acceptProposedAction();
  } else {
    drag_entered_ = false;
    event->ignore();
  }
}

void GraphicsFileView::dragMoveEvent(QDragMoveEvent* event)
{
  if (event->mimeData() != nullptr && event->mimeData()->hasUrls()) {
    drag_entered_ = true;
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
  // Clear folder drop-target highlights. Do NOT call QGraphicsView::dragLeaveEvent:
  // the base implementation requires lastDragDropEvent from its own
  // dragEnterEvent/dragMoveEvent path; we handle DnD on the view without that
  // and calling the base logs:
  //   "QGraphicsView::dragLeaveEvent: drag leave received before drag enter"
  drag_entered_ = false;
  for (GraphicsFileItem* it : items_) {
    if (it != nullptr) {
      it->set_drop_target(false);
    }
  }
  if (event != nullptr) {
    event->accept();
  }
}

void GraphicsFileView::dropEvent(QDropEvent* event)
{
  drag_entered_ = false;
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

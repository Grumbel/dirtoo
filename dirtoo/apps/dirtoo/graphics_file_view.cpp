// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "graphics_file_view.hpp"
#include "group_header_paint.hpp"
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
#include <cmath>
#include <cstdint>

namespace dirtoo::app {
namespace {

/// Map file size to a tile scale factor (log2). ~1 MiB → 1.0; clamped.
[[nodiscard]] double tile_scale_for_bytes(std::uint64_t bytes)
{
  const double lg = std::log2(static_cast<double>(std::max<std::uint64_t>(bytes, 1)));
  const double s = 0.40 + lg * 0.035;
  return std::clamp(s, 0.55, 1.85);
}

} // namespace


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
  setFocusPolicy(Qt::StrongFocus);

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
    connect(model_, &QAbstractItemModel::rowsInserted, this, &GraphicsFileView::on_rows_inserted);
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

void GraphicsFileView::set_relative_size(bool on)
{
  if (relative_size_ == on) {
    return;
  }
  relative_size_ = on;
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
  cursor_row_ = -1;
  selection_anchor_row_ = -1;
  pending_single_select_row_ = -1;
  press_row_ = -1;
  shift_paint_active_ = false;
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
  slot_tile_size_.assign(static_cast<std::size_t>(rows), tile_size_);
  if (rows == 0) {
    layout_cols_ = 1;
    layout_max_row_ = 0;
    scene_->setSceneRect(QRectF());
    return;
  }

  const int band_h = group_header_height(QFontMetrics(font()));
  const int vp_w = std::max(1, viewport()->width());

  if (relative_size_) {
    // Flow left-to-right with per-item tile sizes (log-scaled by file size).
    qreal x = padding_;
    qreal y = padding_;
    qreal row_h = 0;
    int grid_row = 0;
    layout_cols_ = 1;
    for (int i = 0; i < rows; ++i) {
      bool group_start = false;
      QSize tile = tile_size_;
      if (model_ != nullptr) {
        const QModelIndex idx = model_->index(i, 0);
        group_start = idx.data(IsGroupStartRole).toBool()
                      && !idx.data(GroupLabelRole).toString().isEmpty();
        if (const auto* fi = model_->file_at(i)) {
          const double s = tile_scale_for_bytes(fi->size());
          tile = QSize(std::max(48, static_cast<int>(tile_size_.width() * s)),
                       std::max(48, static_cast<int>(tile_size_.height() * s)));
        }
      }
      slot_tile_size_[static_cast<std::size_t>(i)] = tile;

      if (group_start && i > 0) {
        x = padding_;
        y += row_h + spacing_ + band_h + 2;
        row_h = 0;
        ++grid_row;
      } else if (i > 0 && x + tile.width() > vp_w - padding_) {
        x = padding_;
        y += row_h + spacing_;
        row_h = 0;
        ++grid_row;
      }
      if (group_start && i == 0) {
        y += band_h + 2;
      } else if (group_start && i > 0) {
        // band already folded into y advance above
      }

      slot_pos_[static_cast<std::size_t>(i)] = QPointF(x, y);
      x += tile.width() + spacing_;
      row_h = std::max(row_h, static_cast<qreal>(tile.height()));
    }
    layout_max_row_ = grid_row;
    scene_->setSceneRect(0, 0, vp_w, y + row_h + padding_);
    return;
  }

  const int cols = column_count();
  const int cell_w = cell_width();
  const int cell_h = cell_height();
  layout_cols_ = cols;

  // Grid content width (tiles left-aligned inside the grid); center the grid
  // in the viewport so left/right padding match (dirtoo-py center_x_off).
  const int grid_w = cols * cell_w - spacing_ + 2 * padding_;
  const int content_w = std::max(grid_w, vp_w);
  const int center_x_off = std::max(0, (content_w - grid_w) / 2);

  int col = 0;
  int grid_row = 0;
  int max_row = 0;
  int band_accum = 0;
  for (int i = 0; i < rows; ++i) {
    bool group_start = false;
    if (model_ != nullptr) {
      const QModelIndex idx = model_->index(i, 0);
      group_start = idx.data(IsGroupStartRole).toBool()
                    && !idx.data(GroupLabelRole).toString().isEmpty();
      if (group_start && i > 0 && col != 0) {
        col = 0;
        ++grid_row;
      }
    }
    if (group_start) {
      band_accum += band_h + 2;
    }
    slot_pos_[static_cast<std::size_t>(i)] =
        QPointF(center_x_off + padding_ + col * cell_w,
                padding_ + grid_row * cell_h + band_accum);
    slot_tile_size_[static_cast<std::size_t>(i)] = tile_size_;
    max_row = std::max(max_row, grid_row);
    ++col;
    if (col >= cols) {
      col = 0;
      ++grid_row;
    }
  }
  layout_max_row_ = max_row;
  scene_->setSceneRect(0, 0, content_w,
                       padding_ * 2 + (max_row + 1) * cell_h + band_accum);
}

void GraphicsFileView::drawForeground(QPainter* painter, const QRectF& rect)
{
  QGraphicsView::drawForeground(painter, rect);
  if (model_ == nullptr || painter == nullptr || slot_pos_.empty()) {
    return;
  }
  const int band_h = group_header_height(QFontMetrics(font()));
  const int vp_w = viewport()->width();
  const QPointF top_left = mapToScene(0, 0);
  const qreal left = top_left.x();
  for (int i = 0; i < static_cast<int>(slot_pos_.size()); ++i) {
    const QModelIndex idx = model_->index(i, 0);
    if (!idx.data(IsGroupStartRole).toBool()) {
      continue;
    }
    const QString label = idx.data(GroupLabelRole).toString();
    if (label.isEmpty()) {
      continue;
    }
    const qreal item_y = slot_pos_[static_cast<std::size_t>(i)].y();
    const QRectF header_scene(left, item_y - band_h, vp_w, band_h);
    if (!header_scene.intersects(rect)) {
      continue;
    }
    paint_group_header(painter, header_scene.toRect(), label, palette(), font());
  }
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
    const QSize tile =
        (static_cast<std::size_t>(r) < slot_tile_size_.size())
            ? slot_tile_size_[static_cast<std::size_t>(r)]
            : tile_size_;
    if (item == nullptr) {
      item = new GraphicsFileItem(model_, r, this);
      item->set_tile_size(tile);
      scene_->addItem(item);
      if (selected_row_set_.contains(r)) {
        item->setSelected(true);
      }
    } else {
      item->set_row(r);
      item->set_tile_size(tile);
      item->setSelected(selected_row_set_.contains(r));
    }
    item->setPos(slot_pos_[static_cast<std::size_t>(r)]);
  }
  suppress_selection_signal_ = false;
  emit visible_window_changed();
}

void GraphicsFileView::rebuild_items()
{
  clear_all_items();
  selected_row_set_.clear();
  selection_anchor_row_ = -1;
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

void GraphicsFileView::on_model_reset()
{
  rebuild_items();
}

void GraphicsFileView::on_rows_inserted(const QModelIndex& parent, int first, int last)
{
  (void)parent;
  (void)first;
  (void)last;
  if (model_ == nullptr) {
    return;
  }
  // Search (and similar) appends rows without a full model reset. Grow the sparse
  // item vector and relayout without clearing selection or destroying tiles.
  const int rows = model_->rowCount();
  if (static_cast<int>(items_.size()) < rows) {
    items_.resize(static_cast<std::size_t>(rows), nullptr);
  }
  compute_layout_slots();
  update_visible_window();
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

} // namespace dirtoo::app

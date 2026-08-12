// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QGraphicsView>
#include <QModelIndex>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QSet>
#include <vector>

class QGraphicsScene;
class QAbstractItemModel;

namespace dirtoo::app {

class FileListModel;
class GraphicsFileItem;

/// Icon view backed by QGraphicsScene with viewport-windowed tiles for large dirs.
///
/// ## Selection & keyboard cursor (intended behaviour)
///
/// Two independent concepts:
///
/// 1. **Multi-selection** (`selected_row_set_` + live `QGraphicsItem::isSelected`):
///    the set of files that operations (delete, copy, drag, …) act on.
/// 2. **Keyboard cursor** (`cursor_row_`): a single focus tile drawn as a light
///    outline. Independent of multi-selection (Python `FileView._cursor_item`).
///
/// ### Mouse
/// - Empty background + drag → rubber-band select (Qt default).
/// - Left-click unselected tile (no mod) → select only that row, set cursor.
/// - Left-click already-selected tile (no mod) → **keep** the multi-selection
///   until mouse release without a drag; on release without drag, collapse to
///   that single row. If the user starts a drag, the whole multi-selection is
///   the payload. This prevents "clicking one selected item deselects the rest
///   before the drag can start".
/// - Ctrl+click → toggle that row in the multi-selection; set cursor.
/// - Shift+click → select the contiguous row range from the selection anchor
///   (last plain click / Space toggle / keyboard paint seed) to the clicked
///   row; set cursor.
/// - Right-click on selected → keep multi-selection; on unselected → select
///   only that row. Context menu uses the resulting selection.
///
/// ### Keyboard
/// - Arrow keys move the cursor (geometric neighbour, grid fallback). They do
///   **not** change selection unless Shift is held.
/// - Shift+arrow: "paint" mode — first step toggles the current cursor row and
///   remembers that value; further steps apply the same select/deselect while
///   moving. Released Shift ends paint mode.
/// - Space toggles selection of the cursor row and seeds paint mode.
/// - Escape clears multi-selection; cursor is kept so navigation continues.
/// - Enter/Return activates the cursor row.
///
/// ### Cursor vs scroll (warp policy)
/// - If the cursor is already on-screen and a move would leave the viewport,
///   scroll to follow (follow mode).
/// - If the cursor is off-screen, **do not** yank the viewport: warp the cursor
///   onto a visible row instead.
/// - `cursor_move(0, 0)` only seeds when there is no cursor; it never jumps to a
///   neighbour.
///
/// ### Drag & drop
/// - Payload is built **only** from `selected_row_set_` (not transient scene
///   selection residue).
/// - Press on an unselected tile without Ctrl/Shift collapses selection to that
///   row before the drag starts so unrelated files are never dragged.
///
/// Off-window selected rows stay in `selected_row_set_` when tiles are
/// destroyed by the viewport window; they are restored when the tile returns.
class GraphicsFileView : public QGraphicsView {
  Q_OBJECT

public:
  explicit GraphicsFileView(QWidget* parent = nullptr);

  void set_model(FileListModel* model);
  [[nodiscard]] FileListModel* file_model() const noexcept { return model_; }

  void set_tile_size(const QSize& size);
  [[nodiscard]] QSize tile_size() const noexcept { return tile_size_; }

  void set_compact(bool compact);
  /// When true, tile size scales with FileInfo::size() (log2), clamped.
  void set_relative_size(bool on);
  [[nodiscard]] bool relative_size() const { return relative_size_; }
  [[nodiscard]] bool compact() const noexcept { return compact_; }

  void relayout();
  void sync_from_model();

  /// Model rows whose layout slots intersect the current viewport (plus margin).
  /// Uses precomputed slot_pos_ — independent of which GraphicsFileItems are
  /// materialized. Used for thumbnail requests so resize/scroll match layout.
  [[nodiscard]] std::vector<int> viewport_model_rows(qreal margin_px = -1.0) const;

  [[nodiscard]] QModelIndex index_at(const QPoint& view_pos) const;
  [[nodiscard]] std::vector<int> selected_rows() const;
  void clear_selection();
  void select_row(int row, bool clear_others = true);
  void select_all();

  /// Keyboard "file cursor" (dirtoo-py FileView._cursor_item): independent of
  /// multi-selection; arrow keys move it; Shift extends selection.
  [[nodiscard]] int cursor_row() const noexcept { return cursor_row_; }
  [[nodiscard]] bool is_cursor_row(int row) const noexcept { return row >= 0 && row == cursor_row_; }
  void set_cursor_row(int row, bool ensure_visible = true);
  void clear_cursor();
  void cursor_move(int dx, int dy);

  void notify_activated(const QModelIndex& index);
  void notify_middle_clicked(const QModelIndex& index);
  void notify_context_menu(const QPoint& global_pos, const QModelIndex& index);

signals:
  void activated(const QModelIndex& index);
  void middle_clicked(const QModelIndex& index);
  void context_menu_requested(const QPoint& global_pos, const QModelIndex& index);
  void selection_changed();
  /// dest_dir empty → current location (MainWindow decides)
  void files_dropped(const QList<QUrl>& urls, Qt::DropAction action, const QString& dest_dir);
  /// Emitted after the sparse tile window is recomputed (scroll, resize, relayout).
  /// MainWindow uses this to request thumbnails for newly visible rows.
  void visible_window_changed();

protected:
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  void scrollContentsBy(int dx, int dy) override;
  void changeEvent(QEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void focusInEvent(QFocusEvent* event) override;
  void drawForeground(QPainter* painter, const QRectF& rect) override;

private slots:
  void on_model_reset();
  void on_rows_inserted(const QModelIndex& parent, int first, int last);
  void on_model_data_changed(const QModelIndex& top_left, const QModelIndex& bottom_right,
                             const QList<int>& roles);
  void on_scene_selection_changed();

private:
  void rebuild_items();
  void compute_layout_slots();
  void update_visible_window();
  void clear_all_items();
  [[nodiscard]] int cell_width() const;
  [[nodiscard]] int cell_height() const;
  [[nodiscard]] int column_count() const;

  FileListModel* model_ = nullptr;
  QGraphicsScene* scene_ = nullptr;
  /// Sparse: nullptr for rows outside the viewport window.
  std::vector<GraphicsFileItem*> items_;
  /// Precomputed top-left of each row's tile (group-aware).
  std::vector<QPointF> slot_pos_;
  std::vector<QSize> slot_tile_size_;
  QSize tile_size_{128, 160};
  bool compact_ = false;
  bool relative_size_ = false;
  int spacing_ = 12;
  int padding_ = 8;
  bool suppress_selection_signal_ = false;
  /// True after an accepted dragEnter; avoids Qt "drag leave before enter" warnings.
  bool drag_entered_ = false;
  QPoint drag_start_pos_;
  bool drag_started_ = false;
  int layout_cols_ = 1;
  int layout_max_row_ = 0;
  /// Rows selected even when their tile is outside the viewport window.
  mutable QSet<int> selected_row_set_;
  /// Keyboard focus tile (−1 = none). Drawn as a light outline over selection.
  int cursor_row_ = -1;
  /// Shift+arrow "paint" selection: apply this value while Shift is held.
  bool shift_paint_active_ = false;
  bool shift_paint_select_ = false;
  /// Anchor row for Shift+click range selection (−1 = none).
  int selection_anchor_row_ = -1;

  /// Deferred single-select: press landed on an already-selected tile without
  /// Ctrl/Shift. Multi-selection is preserved until release-without-drag, then
  /// collapsed to this row. −1 = no pending collapse.
  int pending_single_select_row_ = -1;
  /// Row under the press that started a potential item drag (−1 = none).
  int press_row_ = -1;

  void start_drag();
  void set_row_selected(int row, bool selected);
  void shift_paint_step(int dx, int dy);
  void ensure_cursor_visible();
  [[nodiscard]] bool is_row_on_screen(int row) const;
  /// If cursor is off-screen, move it to a visible row without scrolling.
  int warp_cursor_to_visible();
  void update_cursor_item_visuals(int old_row, int new_row);
  void select_range(int from_row, int to_row, bool clear_others);
  void apply_click_selection(int row, Qt::KeyboardModifiers mods);
  void materialize_row(int row);
};

} // namespace dirtoo::app

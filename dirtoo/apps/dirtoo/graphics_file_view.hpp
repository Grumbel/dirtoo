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
class GraphicsFileView : public QGraphicsView {
  Q_OBJECT

public:
  explicit GraphicsFileView(QWidget* parent = nullptr);

  void set_model(FileListModel* model);
  [[nodiscard]] FileListModel* file_model() const noexcept { return model_; }

  void set_tile_size(const QSize& size);
  [[nodiscard]] QSize tile_size() const noexcept { return tile_size_; }

  void set_compact(bool compact);
  [[nodiscard]] bool compact() const noexcept { return compact_; }

  void relayout();
  void sync_from_model();

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
  void focusInEvent(QFocusEvent* event) override;

private slots:
  void on_model_reset();
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
  QSize tile_size_{128, 160};
  bool compact_ = false;
  int spacing_ = 12;
  int padding_ = 8;
  bool suppress_selection_signal_ = false;
  QPoint drag_start_pos_;
  bool drag_started_ = false;
  int layout_cols_ = 1;
  int layout_max_row_ = 0;
  /// Rows selected even when their tile is outside the viewport window.
  mutable QSet<int> selected_row_set_;
  /// Keyboard focus tile (−1 = none). Drawn as a light outline over selection.
  int cursor_row_ = -1;

  void start_drag();
  void ensure_cursor_visible();
  void update_cursor_item_visuals(int old_row, int new_row);
};

} // namespace dirtoo::app

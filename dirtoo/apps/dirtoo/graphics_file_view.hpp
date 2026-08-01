// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QGraphicsView>
#include <QModelIndex>
#include <QPoint>
#include <QSize>
#include <vector>

class QGraphicsScene;
class QAbstractItemModel;

namespace dirtoo::app {

class FileListModel;
class GraphicsFileItem;

/// Icon / small-icon view backed by QGraphicsScene (Python FileView parity path).
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
  void dropEvent(QDropEvent* event) override;

private slots:
  void on_model_reset();
  void on_model_data_changed(const QModelIndex& top_left, const QModelIndex& bottom_right,
                             const QList<int>& roles);
  void on_scene_selection_changed();

private:
  void rebuild_items();
  void layout_items();
  void grow_items_batch();

  FileListModel* model_ = nullptr;
  QGraphicsScene* scene_ = nullptr;
  std::vector<GraphicsFileItem*> items_;
  QSize tile_size_{128, 160};
  bool compact_ = false;
  int spacing_ = 12;
  int padding_ = 8;
  bool suppress_selection_signal_ = false;
  QPoint drag_start_pos_;
  bool drag_started_ = false;
  int pending_grow_target_ = 0;
  bool grow_scheduled_ = false;

  void start_drag();
};

} // namespace dirtoo::app

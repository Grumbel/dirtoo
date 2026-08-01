// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QGraphicsItem>
#include <QModelIndex>
#include <QPixmap>
#include <QString>

namespace dirtoo::app {

class FileListModel;
class GraphicsFileView;

/// One tile in the graphics icon scene (Python FileItem analogue).
class GraphicsFileItem : public QGraphicsItem {
public:
  GraphicsFileItem(FileListModel* model, int row, GraphicsFileView* view);

  void set_row(int row);
  [[nodiscard]] int row() const noexcept { return row_; }
  [[nodiscard]] QModelIndex model_index() const;

  void set_tile_size(const QSize& size);
  [[nodiscard]] QSize tile_size() const noexcept { return tile_size_; }

  void set_drop_target(bool on);
  [[nodiscard]] bool is_drop_target() const noexcept { return drop_target_; }

  [[nodiscard]] QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

protected:
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
  FileListModel* model_ = nullptr;
  GraphicsFileView* view_ = nullptr;
  int row_ = -1;
  QSize tile_size_{128, 160};
  bool drop_target_ = false;
};

} // namespace dirtoo::app

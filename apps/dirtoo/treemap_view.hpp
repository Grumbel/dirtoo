// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/tree/fs_tree_node.hpp"
#include "dirtoo/tree/treemap_layout.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

namespace dirtoo::app {

/// Squarified treemap of one FsTreeNode level (children as cells).
///
/// Uses sizes from the tree snapshot only — no filesystem I/O on the GUI thread.
/// Double-click a directory cell to activate its path (navigate / drill).
class TreemapView : public QWidget {
  Q_OBJECT
public:
  explicit TreemapView(QWidget* parent = nullptr);

  /// Show children of @p root. Null clears the view.
  void set_root(std::shared_ptr<const dirtoo::tree::FsTreeNode> root);
  void clear();

  [[nodiscard]] std::shared_ptr<const dirtoo::tree::FsTreeNode> root() const { return root_; }

signals:
  void path_activated(const QString& path);
  void path_hovered(const QString& path, quint64 total_size);

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
  struct Cell {
    dirtoo::tree::TreemapCell layout;
    std::shared_ptr<const dirtoo::tree::FsTreeNode> node;
  };

  void rebuild_layout();
  [[nodiscard]] int cell_at(const QPoint& pos) const;

  std::shared_ptr<const dirtoo::tree::FsTreeNode> root_;
  std::vector<Cell> cells_;
  int hover_index_ = -1;
};

} // namespace dirtoo::app

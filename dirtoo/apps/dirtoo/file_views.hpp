// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "drag_action_overlay.hpp"

#include <QListView>
#include <QTreeView>

namespace dirtoo::app {

/// QTreeView that shows a Copy/Move/Link action overlay during drags.
class FileTreeView : public QTreeView {
  Q_OBJECT
public:
  using QTreeView::QTreeView;

protected:
  void startDrag(Qt::DropActions supportedActions) override
  {
    begin_drag_action_overlay();
    QTreeView::startDrag(supportedActions);
  }
};

/// QListView that shows a Copy/Move/Link action overlay during drags.
class FileListView : public QListView {
  Q_OBJECT
public:
  using QListView::QListView;

protected:
  void startDrag(Qt::DropActions supportedActions) override
  {
    begin_drag_action_overlay();
    QListView::startDrag(supportedActions);
  }
};

} // namespace dirtoo::app

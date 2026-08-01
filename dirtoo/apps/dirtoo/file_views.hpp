// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "drag_action_overlay.hpp"

#include <QDrag>
#include <QListView>
#include <QMimeData>
#include <QPixmap>
#include <QTreeView>

namespace dirtoo::app {
namespace {

inline void apply_dnd_cursors(QDrag* drag)
{
  if (drag == nullptr) {
    return;
  }
  auto load = [](const char* alias) -> QPixmap {
    return QPixmap(QStringLiteral(":/icons/%1").arg(QLatin1String(alias)));
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

} // namespace

/// QTreeView that shows themed DnD cursors + Copy/Move/Link action overlay.
class FileTreeView : public QTreeView {
  Q_OBJECT
public:
  using QTreeView::QTreeView;

protected:
  void startDrag(Qt::DropActions supportedActions) override
  {
    begin_drag_action_overlay();
    const QModelIndexList indexes = selectedIndexes();
    if (indexes.isEmpty() || model() == nullptr) {
      return;
    }
    QMimeData* mime = model()->mimeData(indexes);
    if (mime == nullptr) {
      return;
    }
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    apply_dnd_cursors(drag);
    if (const QVariant dec = indexes.first().data(Qt::DecorationRole); dec.canConvert<QIcon>()) {
      const QIcon icon = dec.value<QIcon>();
      const QPixmap pm = icon.pixmap(QSize(48, 48));
      if (!pm.isNull()) {
        drag->setPixmap(pm);
        drag->setHotSpot(QPoint(pm.width() / 2, pm.height() / 2));
      }
    }
    drag->exec(supportedActions, defaultDropAction());
  }
};

/// QListView that shows themed DnD cursors + Copy/Move/Link action overlay.
class FileListView : public QListView {
  Q_OBJECT
public:
  using QListView::QListView;

protected:
  void startDrag(Qt::DropActions supportedActions) override
  {
    begin_drag_action_overlay();
    const QModelIndexList indexes = selectedIndexes();
    if (indexes.isEmpty() || model() == nullptr) {
      return;
    }
    QMimeData* mime = model()->mimeData(indexes);
    if (mime == nullptr) {
      return;
    }
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    apply_dnd_cursors(drag);
    if (const QVariant dec = indexes.first().data(Qt::DecorationRole); dec.canConvert<QIcon>()) {
      const QIcon icon = dec.value<QIcon>();
      const QPixmap pm = icon.pixmap(QSize(64, 64));
      if (!pm.isNull()) {
        drag->setPixmap(pm);
        drag->setHotSpot(QPoint(pm.width() / 2, pm.height() / 2));
      }
    }
    drag->exec(supportedActions, defaultDropAction());
  }
};

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "drag_action_overlay.hpp"
#include "badge_icons.hpp"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDragMoveEvent>
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


inline Qt::DropAction drop_action_from_event(const QDropEvent* event)
{
  const auto mods = event->modifiers();
  if ((mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier)) {
    return Qt::LinkAction;
  }
  if (mods & Qt::AltModifier) {
    return Qt::LinkAction;
  }
  if (mods & Qt::ShiftModifier) {
    return Qt::MoveAction;
  }
  if (mods & Qt::ControlModifier) {
    return Qt::CopyAction;
  }
  const Qt::DropAction proposed = event->proposedAction();
  if (proposed == Qt::CopyAction || proposed == Qt::MoveAction || proposed == Qt::LinkAction) {
    return proposed;
  }
  return Qt::CopyAction;
}

inline Qt::DropAction default_drag_action()
{
  const auto mods = QApplication::keyboardModifiers();
  if ((mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier)) {
    return Qt::LinkAction;
  }
  if (mods & Qt::AltModifier) {
    return Qt::LinkAction;
  }
  if (mods & Qt::ShiftModifier) {
    return Qt::MoveAction;
  }
  if (mods & Qt::ControlModifier) {
    return Qt::CopyAction;
  }
  return Qt::CopyAction;
}

/// Column-0 indexes only (SelectRows yields every column).
inline QModelIndexList column0_indexes(const QModelIndexList& all)
{
  QModelIndexList out;
  out.reserve(all.size());
  for (const QModelIndex& idx : all) {
    if (idx.isValid() && idx.column() == 0) {
      out.append(idx);
    }
  }
  return out;
}

inline bool accept_url_drag(QDragEnterEvent* event)
{
  if (event->mimeData() != nullptr && event->mimeData()->hasUrls()) {
    event->setDropAction(drop_action_from_event(event));
    event->accept();
    return true;
  }
  event->ignore();
  return false;
}

inline bool accept_url_drag_move(QDragMoveEvent* event)
{
  if (event->mimeData() != nullptr && event->mimeData()->hasUrls()) {
    event->setDropAction(drop_action_from_event(event));
    event->accept();
    return true;
  }
  event->ignore();
  return false;
}

} // namespace

/// QTreeView that shows themed DnD cursors + Copy/Move/Link action overlay.
class FileTreeView : public QTreeView {
public:
  using QTreeView::QTreeView;

protected:
  void startDrag(Qt::DropActions supportedActions) override
  {
    begin_drag_action_overlay();
    if (model() == nullptr) {
      return;
    }
    const QModelIndexList indexes = column0_indexes(selectedIndexes());
    if (indexes.isEmpty()) {
      return;
    }
    QMimeData* mime = model()->mimeData(indexes);
    if (mime == nullptr || !mime->hasUrls()) {
      delete mime;
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
    const Qt::DropActions actions = supportedActions | Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
    drag->exec(actions, default_drag_action());
  }

  void dragEnterEvent(QDragEnterEvent* event) override
  {
    accept_url_drag(event);
  }

  void dragMoveEvent(QDragMoveEvent* event) override
  {
    accept_url_drag_move(event);
  }

  void dropEvent(QDropEvent* event) override
  {
    if (event->mimeData() == nullptr || !event->mimeData()->hasUrls() || model() == nullptr) {
      event->ignore();
      return;
    }
    const Qt::DropAction action = drop_action_from_event(event);
    const QModelIndex idx = indexAt(event->position().toPoint());
    // OnItem: pass the directory index as parent so the model resolves dest_dir.
    const QModelIndex parent = idx.isValid() ? idx : QModelIndex();
    if (model()->dropMimeData(event->mimeData(), action, -1, 0, parent)) {
      event->setDropAction(action);
      event->accept();
    } else {
      event->ignore();
    }
  }
};

/// QListView that shows themed DnD cursors + Copy/Move/Link action overlay.
class FileListView : public QListView {
public:
  using QListView::QListView;

protected:
  void startDrag(Qt::DropActions supportedActions) override
  {
    begin_drag_action_overlay();
    if (model() == nullptr) {
      return;
    }
    const QModelIndexList indexes = column0_indexes(selectedIndexes());
    if (indexes.isEmpty()) {
      return;
    }
    QMimeData* mime = model()->mimeData(indexes);
    if (mime == nullptr || !mime->hasUrls()) {
      delete mime;
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
    const Qt::DropActions actions = supportedActions | Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
    drag->exec(actions, default_drag_action());
  }

  void dragEnterEvent(QDragEnterEvent* event) override
  {
    accept_url_drag(event);
  }

  void dragMoveEvent(QDragMoveEvent* event) override
  {
    accept_url_drag_move(event);
  }

  void dropEvent(QDropEvent* event) override
  {
    if (event->mimeData() == nullptr || !event->mimeData()->hasUrls() || model() == nullptr) {
      event->ignore();
      return;
    }
    const Qt::DropAction action = drop_action_from_event(event);
    const QModelIndex idx = indexAt(event->position().toPoint());
    const QModelIndex parent = idx.isValid() ? idx : QModelIndex();
    if (model()->dropMimeData(event->mimeData(), action, -1, 0, parent)) {
      event->setDropAction(action);
      event->accept();
    } else {
      event->ignore();
    }
  }
};

} // namespace dirtoo::app

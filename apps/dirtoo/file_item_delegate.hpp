// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStyledItemDelegate>
#include <QModelIndex>
#include <QPoint>
#include <QString>

namespace dirtoo::app {

class FileListModel;

/// Paints icon-view tiles with media overlays (WxH, duration, fps) and type badges
/// (image / video), similar to the Python FileItemRenderer.
class FileItemDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit FileItemDelegate(FileListModel* model, QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                               const QModelIndex& index) const override;

  bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

signals:
  void tag_chip_clicked(const QString& tag_name);
  /// Right-click on a tag chip (global_pos for popup; index is the file tile).
  void tag_chip_menu_requested(const QString& tag_name, const QPoint& global_pos,
                               const QModelIndex& index);
  void group_header_clicked(const QString& label);

private:
  [[nodiscard]] QRect thumb_rect_for(const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const;
  FileListModel* model_ = nullptr;
};

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/collection/file_collection.hpp"
#include "dirtoo/fs/file_info.hpp"

#include <QAbstractTableModel>
#include <QHash>
#include <QIcon>
#include <QUrl>

namespace dirtoo::app {

enum class FileListColumn {
  Name = 0,
  Size,
  Modified,
  Type,
  Count
};

/// Qt model over the visible slice of a FileCollection.
class FileListModel : public QAbstractTableModel {
  Q_OBJECT

public:
  explicit FileListModel(QObject* parent = nullptr);

  void set_collection(collection::FileCollection* collection);
  void refresh();

  void set_thumbnail(const QString& path, const QIcon& icon);
  void clear_thumbnails();

  /// Icon-view caption density (0=none … 4=name+size+date). Detail view ignores this.
  void set_icon_style(bool enabled);
  void set_icon_detail_level(int level);
  [[nodiscard]] int icon_detail_level() const noexcept { return icon_detail_level_; }
  [[nodiscard]] bool icon_style_active() const noexcept { return icon_style_; }
  [[nodiscard]] static constexpr int icon_detail_level_min() { return 0; }
  [[nodiscard]] static constexpr int icon_detail_level_max() { return 4; }
  /// Extra text rows under the icon for current LOD (Python k map).
  [[nodiscard]] int icon_text_rows() const noexcept;

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                    int role = Qt::DisplayRole) const override;
  [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

  [[nodiscard]] Qt::DropActions supportedDropActions() const override;
  [[nodiscard]] QStringList mimeTypes() const override;
  [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;
  [[nodiscard]] bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                                     int column, const QModelIndex& parent) const override;
  bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                    const QModelIndex& parent) override;

  [[nodiscard]] const fs::FileInfo* file_at(int row) const;
  [[nodiscard]] std::vector<fs::FileInfo> files_at(const QModelIndexList& indexes) const;

signals:
  /// Emitted when external URLs are dropped onto the view. Handled by MainWindow.
  void urls_dropped(const QList<QUrl>& urls, Qt::DropAction action);

private:
  [[nodiscard]] QIcon icon_for(const fs::FileInfo& fi) const;

  collection::FileCollection* collection_ = nullptr;
  QHash<QString, QIcon> thumbnails_;
  bool icon_style_ = false;
  int icon_detail_level_ = 3; // name + size by default (Python-ish)
};

} // namespace dirtoo::app

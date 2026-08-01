// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/collection/file_collection.hpp"
#include "dirtoo/fs/file_info.hpp"

#include <QAbstractTableModel>
#include <QHash>
#include <QIcon>

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

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                    int role = Qt::DisplayRole) const override;
  [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

  [[nodiscard]] const fs::FileInfo* file_at(int row) const;
  [[nodiscard]] std::vector<fs::FileInfo> files_at(const QModelIndexList& indexes) const;

private:
  [[nodiscard]] QIcon icon_for(const fs::FileInfo& fi) const;

  collection::FileCollection* collection_ = nullptr;
  QHash<QString, QIcon> thumbnails_;
};

} // namespace dirtoo::app

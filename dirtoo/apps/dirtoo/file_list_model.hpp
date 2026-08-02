// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/collection/file_collection.hpp"
#include "dirtoo/fs/file_info.hpp"

#include <QAbstractTableModel>
#include <QHash>
#include <QSet>
#include <QIcon>
#include <QUrl>

namespace dirtoo::app {

enum class FileListColumn {
  Name = 0,
  Size,
  Width,       ///< media width (px)
  Height,      ///< media height (px)
  Dimensions,  ///< width×height
  Framerate,   ///< fps
  Duration,    ///< media duration (h:mm:ss)
  Modified,
  Type,
  Count
};

enum FileListRole {
  PathRole = Qt::UserRole,
  GroupLabelRole = Qt::UserRole + 1,
  IsGroupStartRole = Qt::UserRole + 2,
  ThumbnailStatusRole = Qt::UserRole + 3,
  AccessDeniedRole = Qt::UserRole + 4,
  IsNewRole = Qt::UserRole + 5,
  /// Seconds between this row and previous (0 if none / disabled).
  TimeGapSecondsRole = Qt::UserRole + 6,
  /// Non-recursive child entry count for directories (-1 unknown / pending).
  ChildCountRole = Qt::UserRole + 7,
};

enum class ThumbnailStatus {
  None = 0,
  Pending,
  Ready,
  Failed,
};

/// Qt model over the visible slice of a FileCollection.
class FileListModel : public QAbstractTableModel {
  Q_OBJECT

public:
  explicit FileListModel(QObject* parent = nullptr);

  void set_collection(collection::FileCollection* collection);
  void refresh();

  void set_thumbnail(const QString& path, const QIcon& icon);
  void set_thumbnail_pending(const QString& path);
  void set_thumbnail_failed(const QString& path);
  void clear_thumbnails();
  void clear_thumbnail(const QString& path);
  void mark_new(const QString& path);
  void clear_new_marks();
  void prune_new_marks(const QSet<QString>& keep_paths);

  [[nodiscard]] ThumbnailStatus thumbnail_status(const QString& path) const;
  [[nodiscard]] bool is_new(const QString& path) const;

  /// Thread-safe request to refresh a row (queues to this object's thread).
  Q_INVOKABLE void notify_row_changed(int row);

  /// Icon-view caption density (0=none … 4=name+size+date). Detail view ignores this.
  void set_icon_style(bool enabled);
  void set_icon_detail_level(int level);
  [[nodiscard]] int icon_detail_level() const noexcept { return icon_detail_level_; }
  [[nodiscard]] bool icon_style_active() const noexcept { return icon_style_; }
  void set_crop_thumbnails(bool crop);
  void set_show_abspath(bool show);
  [[nodiscard]] bool show_abspath() const noexcept { return show_abspath_; }
  void set_show_timegaps(bool show);
  [[nodiscard]] bool show_timegaps() const noexcept { return show_timegaps_; }
  [[nodiscard]] bool crop_thumbnails() const noexcept { return crop_thumbnails_; }
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

  /// Request a non-recursive directory child count (async). Result via ChildCountRole.
  void request_child_count(const QString& path);
  void clear_child_counts();
  /// Set a known child count (e.g. from archive index).
  void set_child_count(const QString& path, qint64 count);

signals:
  /// Emitted when external URLs are dropped onto the view. Handled by MainWindow.
  /// @param dest_dir empty = current location; otherwise absolute path of folder target.
  void urls_dropped(const QList<QUrl>& urls, Qt::DropAction action, const QString& dest_dir);

private slots:
  void on_child_count_ready(const QString& path, qint64 count);

private:
  [[nodiscard]] QIcon icon_for(const fs::FileInfo& fi) const;
  void emit_path_changed(const QString& path);

  collection::FileCollection* collection_ = nullptr;
  QHash<QString, QIcon> thumbnails_;
  QHash<QString, ThumbnailStatus> thumbnail_status_;
  QSet<QString> new_paths_;
  QHash<QString, qint64> child_counts_; // -1 = pending, >=0 = known
  QSet<QString> child_count_pending_;
  bool icon_style_ = false;
  int icon_detail_level_ = 3; // name + size by default (Python-ish)
  bool crop_thumbnails_ = false;
  bool show_abspath_ = false;
  bool show_timegaps_ = false;
  bool group_refresh_pending_ = false;
};

} // namespace dirtoo::app

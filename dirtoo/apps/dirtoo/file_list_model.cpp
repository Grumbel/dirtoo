// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "file_list_model.hpp"

#include <QTimer>

#include <filesystem>

#include <QDateTime>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QLocale>
#include <QMimeData>
#include <QUrl>

#include <algorithm>
#include <sys/stat.h>

namespace dirtoo::app {
namespace {

QFileIconProvider& icon_provider()
{
  static QFileIconProvider provider;
  return provider;
}

QString format_size(std::uint64_t bytes, bool is_directory)
{
  if (is_directory) {
    return QStringLiteral("—");
  }
  return QLocale::system().formattedDataSize(static_cast<qint64>(bytes));
}

QString format_mtime_from_path(const std::filesystem::path& path)
{
  struct ::stat st {};
  if (::stat(path.c_str(), &st) != 0) {
    return {};
  }
  const QDateTime dt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(st.st_mtime));
  return QLocale::system().toString(dt, QLocale::ShortFormat);
}

QString type_label(const fs::FileInfo& fi)
{
  if (fi.is_directory()) {
    return QStringLiteral("Folder");
  }
  if (fi.is_symlink()) {
    return QStringLiteral("Link");
  }
  const auto ext = fi.extension();
  if (ext.empty()) {
    return QStringLiteral("File");
  }
  if (!ext.empty() && ext[0] == '.') {
    return QString::fromStdString(ext.substr(1));
  }
  return QString::fromStdString(ext);
}

} // namespace

FileListModel::FileListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void FileListModel::set_collection(collection::FileCollection* collection)
{
  beginResetModel();
  collection_ = collection;
  endResetModel();
}

void FileListModel::refresh()
{
  // Prefer layoutChanged over full reset when the collection already backs this model.
  // GraphicsFileView can relayout/reuse items instead of tearing down the scene.
  emit layoutAboutToBeChanged();
  emit layoutChanged();
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
  }
}

void FileListModel::emit_path_changed(const QString& path)
{
  if (collection_ == nullptr) {
    return;
  }
  const auto& visible = collection_->visible_items();
  for (int row = 0; row < static_cast<int>(visible.size()); ++row) {
    if (QString::fromStdString(visible[static_cast<std::size_t>(row)].path().string()) == path) {
      const QModelIndex idx = index(row, 0);
      emit dataChanged(idx, idx,
                       {Qt::DecorationRole, ThumbnailStatusRole, IsNewRole, AccessDeniedRole});
      break;
    }
  }
}

void FileListModel::set_thumbnail(const QString& path, const QIcon& icon)
{
  thumbnails_.insert(path, icon);
  thumbnail_status_.insert(path, ThumbnailStatus::Ready);
  new_paths_.remove(path);
  emit_path_changed(path);
}

void FileListModel::set_thumbnail_pending(const QString& path)
{
  if (thumbnail_status_.value(path, ThumbnailStatus::None) == ThumbnailStatus::Ready) {
    return;
  }
  thumbnail_status_.insert(path, ThumbnailStatus::Pending);
  emit_path_changed(path);
}

void FileListModel::set_thumbnail_failed(const QString& path)
{
  thumbnail_status_.insert(path, ThumbnailStatus::Failed);
  emit_path_changed(path);
}

void FileListModel::mark_new(const QString& path)
{
  new_paths_.insert(path);
  emit_path_changed(path);
}

void FileListModel::clear_new_marks()
{
  if (new_paths_.isEmpty()) {
    return;
  }
  new_paths_.clear();
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {IsNewRole});
  }
}

ThumbnailStatus FileListModel::thumbnail_status(const QString& path) const
{
  return thumbnail_status_.value(path, ThumbnailStatus::None);
}

bool FileListModel::is_new(const QString& path) const
{
  return new_paths_.contains(path);
}

void FileListModel::notify_row_changed(int row)
{
  if (row < 0 || row >= rowCount()) {
    return;
  }
  const QModelIndex idx = index(row, 0);
  emit dataChanged(idx, idx);
  // Duration groups depend on media meta; regroup after a short debounce.
  if (collection_ != nullptr
      && collection_->group_mode() == collection::GroupMode::Duration) {
    if (group_refresh_pending_) {
      return;
    }
    group_refresh_pending_ = true;
    QTimer::singleShot(150, this, [this] {
      group_refresh_pending_ = false;
      if (collection_ == nullptr
          || collection_->group_mode() != collection::GroupMode::Duration) {
        return;
      }
      beginResetModel();
      collection_->refresh_groups();
      endResetModel();
    });
  }
}

void FileListModel::clear_thumbnail(const QString& path)
{
  thumbnails_.remove(path);
  thumbnail_status_.remove(path);
  emit_path_changed(path);
}

void FileListModel::clear_thumbnails()
{
  if (thumbnails_.isEmpty() && thumbnail_status_.isEmpty()) {
    return;
  }
  thumbnails_.clear();
  thumbnail_status_.clear();
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0),
                     {Qt::DecorationRole, ThumbnailStatusRole});
  }
}

void FileListModel::set_crop_thumbnails(bool crop)
{
  if (crop_thumbnails_ == crop) {
    return;
  }
  crop_thumbnails_ = crop;
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {Qt::DecorationRole});
  }
}

void FileListModel::set_show_abspath(bool show)
{
  if (show_abspath_ == show) {
    return;
  }
  show_abspath_ = show;
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {Qt::DisplayRole});
  }
}

void FileListModel::set_icon_style(bool enabled)
{
  if (icon_style_ == enabled) {
    return;
  }
  icon_style_ = enabled;
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {Qt::DisplayRole, Qt::TextAlignmentRole});
  }
}

void FileListModel::set_icon_detail_level(int level)
{
  level = std::clamp(level, icon_detail_level_min(), icon_detail_level_max());
  if (icon_detail_level_ == level) {
    return;
  }
  icon_detail_level_ = level;
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, static_cast<int>(FileListColumn::Count) - 1),
                     {Qt::DisplayRole});
  }
}

int FileListModel::icon_text_rows() const noexcept
{
  // Matches Python IconMode: k = [0, 1, 1, 2, 3][level]
  static constexpr int kRows[] = {0, 1, 1, 2, 3};
  const int lvl = std::clamp(icon_detail_level_, 0, 4);
  return kRows[lvl];
}


QIcon FileListModel::icon_for(const fs::FileInfo& fi) const
{
  const QString path = QString::fromStdString(fi.path().string());
  const auto it = thumbnails_.constFind(path);
  if (it != thumbnails_.constEnd()) {
    return it.value();
  }
  if (fi.is_synthetic()) {
    return icon_provider().icon(fi.is_directory() ? QFileIconProvider::Folder
                                                  : QFileIconProvider::File);
  }
  return icon_provider().icon(QFileInfo(path));
}

int FileListModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid() || collection_ == nullptr) {
    return 0;
  }
  return static_cast<int>(collection_->visible_items().size());
}

int FileListModel::columnCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(FileListColumn::Count);
}

QVariant FileListModel::data(const QModelIndex& index, int role) const
{
  const fs::FileInfo* fi = file_at(index.row());
  if (fi == nullptr) {
    return {};
  }

  if (role == Qt::DecorationRole && index.column() == 0) {
    return icon_for(*fi);
  }

  if (role == Qt::DisplayRole) {
    switch (static_cast<FileListColumn>(index.column())) {
    case FileListColumn::Name: {
      const QString name = show_abspath_
                               ? QString::fromStdString(fi->path().string())
                               : QString::fromStdString(fi->basename());
      if (!icon_style_ || icon_detail_level_ <= 0) {
        return name;
      }
      // Icon captions: LOD mirrors Python FileItemRenderer.paint_text_items
      QString caption = name;
      if (icon_detail_level_ > 2) {
        caption += QLatin1Char('\n');
        caption += format_size(fi->size(), fi->is_directory());
      }
      if (icon_detail_level_ > 3) {
        const QString mtime = format_mtime_from_path(fi->path());
        if (!mtime.isEmpty()) {
          caption += QLatin1Char('\n');
          caption += mtime;
        }
      }
      return caption;
    }
    case FileListColumn::Size:
      return format_size(fi->size(), fi->is_directory());
    case FileListColumn::Modified:
      return format_mtime_from_path(fi->path());
    case FileListColumn::Type:
      return type_label(*fi);
    case FileListColumn::Count:
      break;
    }
  }

  if (role == Qt::TextAlignmentRole) {
    if (index.column() == static_cast<int>(FileListColumn::Size)) {
      return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    }
    if (icon_style_ && index.column() == static_cast<int>(FileListColumn::Name)) {
      return static_cast<int>(Qt::AlignHCenter | Qt::AlignTop);
    }
  }

  if (role == PathRole || role == Qt::UserRole) {
    return QString::fromStdString(fi->path().string());
  }

  if (role == ThumbnailStatusRole) {
    return static_cast<int>(thumbnail_status(QString::fromStdString(fi->path().string())));
  }

  if (role == IsNewRole) {
    return is_new(QString::fromStdString(fi->path().string()));
  }

  if (role == AccessDeniedRole) {
    if (fi->is_synthetic() || fi->path().empty()) {
      return false;
    }
    // No owner read bit → treat as locked (same idea as Python have_access()).
    using perms = std::filesystem::perms;
    const auto p = fi->permissions();
    const bool readable = (p & (perms::owner_read | perms::group_read | perms::others_read))
                          != perms::none;
    return !readable;
  }

  if (role == GroupLabelRole) {
    if (collection_ == nullptr) {
      return {};
    }
    const auto label = collection_->group_label_for(*fi);
    if (label.empty()) {
      return {};
    }
    return QString::fromStdString(label);
  }

  if (role == IsGroupStartRole) {
    if (collection_ == nullptr || collection_->group_mode() == collection::GroupMode::None) {
      return false;
    }
    const auto label = collection_->group_label_for(*fi);
    if (label.empty()) {
      return false;
    }
    if (index.row() == 0) {
      return true;
    }
    const auto* prev = file_at(index.row() - 1);
    if (prev == nullptr) {
      return true;
    }
    return collection_->group_label_for(*prev) != label;
  }

  return {};
}

QVariant FileListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
    return {};
  }
  switch (static_cast<FileListColumn>(section)) {
  case FileListColumn::Name:
    return QStringLiteral("Name");
  case FileListColumn::Size:
    return QStringLiteral("Size");
  case FileListColumn::Modified:
    return QStringLiteral("Modified");
  case FileListColumn::Type:
    return QStringLiteral("Type");
  case FileListColumn::Count:
    break;
  }
  return {};
}

Qt::ItemFlags FileListModel::flags(const QModelIndex& index) const
{
  if (!index.isValid()) {
    return Qt::ItemIsDropEnabled;
  }
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

Qt::DropActions FileListModel::supportedDropActions() const
{
  return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

QStringList FileListModel::mimeTypes() const
{
  return {QStringLiteral("text/uri-list")};
}

QMimeData* FileListModel::mimeData(const QModelIndexList& indexes) const
{
  QList<QUrl> urls;
  std::vector<int> rows;
  for (const QModelIndex& idx : indexes) {
    rows.push_back(idx.row());
  }
  std::sort(rows.begin(), rows.end());
  rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
  for (int row : rows) {
    if (const fs::FileInfo* fi = file_at(row)) {
      urls.push_back(QUrl::fromLocalFile(QString::fromStdString(fi->path().string())));
    }
  }
  if (urls.isEmpty()) {
    return nullptr;
  }
  auto* mime = new QMimeData;
  mime->setUrls(urls);
  return mime;
}

bool FileListModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                                    int column, const QModelIndex& parent) const
{
  (void)row;
  (void)column;
  (void)parent;
  if (data == nullptr) {
    return false;
  }
  if (!(action == Qt::CopyAction || action == Qt::MoveAction || action == Qt::LinkAction)) {
    return false;
  }
  return data->hasUrls();
}

bool FileListModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                 const QModelIndex& parent)
{
  (void)column;
  if (!canDropMimeData(data, action, -1, -1, {})) {
    return false;
  }
  // Prefer dropping onto a directory row when the view reports an item target.
  QString dest_dir;
  QModelIndex target;
  if (parent.isValid()) {
    target = parent.sibling(parent.row(), 0);
  } else if (row >= 0) {
    target = index(row, 0);
  }
  if (target.isValid()) {
    if (const fs::FileInfo* fi = file_at(target.row()); fi != nullptr && fi->is_directory()) {
      dest_dir = QString::fromStdString(fi->path().string());
    }
  }
  emit urls_dropped(data->urls(), action, dest_dir);
  // Model does not mutate itself; MainWindow performs the FS operation.
  return true;
}

const fs::FileInfo* FileListModel::file_at(int row) const
{
  if (collection_ == nullptr || row < 0) {
    return nullptr;
  }
  const auto& visible = collection_->visible_items();
  if (static_cast<std::size_t>(row) >= visible.size()) {
    return nullptr;
  }
  return &visible[static_cast<std::size_t>(row)];
}

std::vector<fs::FileInfo> FileListModel::files_at(const QModelIndexList& indexes) const
{
  std::vector<fs::FileInfo> out;
  std::vector<int> rows;
  rows.reserve(static_cast<std::size_t>(indexes.size()));
  for (const QModelIndex& idx : indexes) {
    rows.push_back(idx.row());
  }
  std::sort(rows.begin(), rows.end());
  rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
  for (int row : rows) {
    if (const fs::FileInfo* fi = file_at(row)) {
      out.push_back(*fi);
    }
  }
  return out;
}

} // namespace dirtoo::app

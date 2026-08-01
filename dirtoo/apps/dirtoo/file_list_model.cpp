// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "file_list_model.hpp"

#include <QDateTime>
#include <QFileIconProvider>
#include <QLocale>

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
  beginResetModel();
  endResetModel();
}

void FileListModel::set_thumbnail(const QString& path, const QIcon& icon)
{
  thumbnails_.insert(path, icon);
  if (collection_ == nullptr) {
    return;
  }
  const auto& visible = collection_->visible_items();
  for (int row = 0; row < static_cast<int>(visible.size()); ++row) {
    if (QString::fromStdString(visible[static_cast<std::size_t>(row)].path().string()) == path) {
      const QModelIndex idx = index(row, 0);
      emit dataChanged(idx, idx, {Qt::DecorationRole});
      break;
    }
  }
}

void FileListModel::clear_thumbnails()
{
  if (thumbnails_.isEmpty()) {
    return;
  }
  thumbnails_.clear();
  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {Qt::DecorationRole});
  }
}

QIcon FileListModel::icon_for(const fs::FileInfo& fi) const
{
  const QString path = QString::fromStdString(fi.path().string());
  const auto it = thumbnails_.constFind(path);
  if (it != thumbnails_.constEnd()) {
    return it.value();
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
    case FileListColumn::Name:
      return QString::fromStdString(fi->basename());
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

  if (role == Qt::TextAlignmentRole && index.column() == static_cast<int>(FileListColumn::Size)) {
    return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
  }

  if (role == Qt::UserRole) {
    return QString::fromStdString(fi->path().string());
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
    return Qt::NoItemFlags;
  }
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
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

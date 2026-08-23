// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "file_list_model.hpp"
#include "archive_member_cache.hpp"
#include "dirtoo/filter/media_meta_cache.hpp"
#include "size_format.hpp"

#include <chrono>
#include <cstdlib>
#include <string>

#include <QTimer>

#include <filesystem>
#include <unistd.h>

#include <QColor>
#include <QDateTime>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QLocale>
#include <QMetaObject>
#include <QMimeData>
#include <QModelIndex>
#include <QStandardPaths>
#include <QThreadPool>
#include <QUrl>

#include <algorithm>
namespace dirtoo::app {

namespace {

QString format_size(std::uint64_t bytes, bool /*is_directory*/)
{
  return format_byte_size(bytes);
}

QString format_sys_time(std::chrono::system_clock::time_point tp)
{
  const auto secs =
      std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
  if (secs <= 0) {
    return {};
  }
  const QDateTime dt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(secs));
  // Human-friendly ISO local date/time: "2011-12-21 16:14"
  return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString format_mtime(std::filesystem::file_time_type mtime)
{
  try {
    const auto sys = std::chrono::clock_cast<std::chrono::system_clock>(mtime);
    return format_sys_time(sys);
  } catch (...) {
    return {};
  }
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
  clear_launch_flash();
  beginResetModel();
  collection_ = collection;
  endResetModel();
}

void FileListModel::refresh()
{
  // Prefer layoutChanged over full reset when the collection already backs this model.
  // GraphicsFileView recomputes viewport window from layoutChanged.
  // Avoid dataChanged over the entire range — that is O(n) and forces every
  // delegate/view to re-query all roles after every sort/filter/watcher merge.
  emit layoutAboutToBeChanged();
  emit layoutChanged();
}

void FileListModel::notify_rows_appended(int count)
{
  if (count <= 0 || collection_ == nullptr) {
    return;
  }
  const int total = static_cast<int>(collection_->visible_items().size());
  const int first = total - count;
  if (first < 0) {
    refresh();
    return;
  }
  beginInsertRows(QModelIndex{}, first, total - 1);
  endInsertRows();
}

void FileListModel::emit_path_changed(const QString& path)
{
  if (collection_ == nullptr) {
    return;
  }
  const auto& visible = collection_->visible_items();
  for (int row = 0; row < static_cast<int>(visible.size()); ++row) {
    if (QString::fromStdString(visible[static_cast<std::size_t>(row)].path().string()) == path) {
      const QModelIndex left = index(row, 0);
      const QModelIndex right =
          index(row, static_cast<int>(FileListColumn::Count) - 1);
      emit dataChanged(left, right,
                       {Qt::DecorationRole, Qt::DisplayRole, Qt::ToolTipRole, ThumbnailStatusRole,
                        IsNewRole, AccessDeniedRole, IsUnreadableRole, IsUnwritableRole,
                        ChildCountRole, LaunchFlashRole, IsOpenedRole});
      break;
    }
  }
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

void FileListModel::set_show_timegaps(bool show)
{
  if (show_timegaps_ == show) {
    return;
  }
  show_timegaps_ = show;
  if (rowCount() > 0) {
    // sizeHint and paint depend on TimeGapSecondsRole
    emit layoutAboutToBeChanged();
    emit layoutChanged();
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

  if (role == Qt::ToolTipRole) {
    QString tip = QString::fromStdString(fi->path().string());
    if (fi->is_symlink()) {
      std::error_code ec;
      const auto target = std::filesystem::read_symlink(fi->path(), ec);
      if (!ec) {
        tip += QStringLiteral("\n→ ");
        tip += QString::fromStdString(target.string());
        std::error_code ec2;
        // exists() follows the symlink; false ⇒ dangling.
        if (!std::filesystem::exists(fi->path(), ec2)) {
          tip += QStringLiteral("\n(broken symlink)");
        }
      } else {
        tip += QStringLiteral("\n(broken symlink)");
      }
    }
    const QString path_key = QString::fromStdString(fi->path().string());
    if (thumbnail_status_.value(path_key, ThumbnailStatus::None) == ThumbnailStatus::Failed) {
      const QString err = thumbnail_errors_.value(path_key);
      tip += QStringLiteral("\nThumbnail failed");
      if (!err.isEmpty()) {
        tip += QStringLiteral(": ");
        tip += err;
      }
    }
    return tip;
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
        const QString mtime = format_mtime(fi->mtime());
        if (!mtime.isEmpty()) {
          caption += QLatin1Char('\n');
          caption += mtime;
        }
      }
      return caption;
    }
    case FileListColumn::Size:
      if (fi->is_directory()) {
        // Metadata size (st_size), same order of magnitude as `ls -l` / `stat`.
        // Child count is additional context when the async probe has finished.
        const QString path = QString::fromStdString(fi->path().string());
        const QString bytes = format_size(fi->size(), true);
        const auto it = child_counts_.constFind(path);
        if (it == child_counts_.constEnd()) {
          const_cast<FileListModel*>(this)->request_child_count(path);
          return bytes;
        }
        if (it.value() < 0) {
          return bytes;
        }
        return QStringLiteral("%1 · %2 items").arg(bytes).arg(it.value());
      }
      return format_size(fi->size(), false);
    case FileListColumn::Width:
    case FileListColumn::Height:
    case FileListColumn::Dimensions:
    case FileListColumn::AspectRatio:
    case FileListColumn::Framerate: {
      if (fi->is_directory()) {
        return {};
      }
      const auto meta = filter::MediaMetaCache::instance().try_get(fi->path());
      if (!meta) {
        // Only enqueue a probe when the path is not yet in the cache (positive or
        // negative). Re-requesting a hit that lacks a field (e.g. image without
        // duration) immediately invokes the ready callback and queues
        // notify_row_changed → infinite data()/paint loop at 100% CPU while
        // ActivityMonitor stays Idle.
        if (!fi->path().empty() && !filter::MediaMetaCache::instance().is_negative(fi->path())) {
          const int row = index.row();
          filter::MediaMetaCache::instance().request(
              fi->path(), 0,
              [this, row](const std::string&, std::optional<filter::MediaInfo>, std::uint64_t) {
                QMetaObject::invokeMethod(const_cast<FileListModel*>(this), "notify_row_changed",
                                          Qt::QueuedConnection, Q_ARG(int, row));
              });
        }
        return {};
      }
      const auto col = static_cast<FileListColumn>(index.column());
      if (col == FileListColumn::Width) {
        return meta->width ? QVariant(static_cast<qulonglong>(*meta->width)) : QVariant{};
      }
      if (col == FileListColumn::Height) {
        return meta->height ? QVariant(static_cast<qulonglong>(*meta->height)) : QVariant{};
      }
      if (col == FileListColumn::Framerate) {
        if (!meta->framerate || *meta->framerate <= 0.0) {
          return {};
        }
        return QStringLiteral("%1").arg(*meta->framerate, 0, 'f', 2);
      }
      if (!meta->width || !meta->height || *meta->width == 0 || *meta->height == 0) {
        return {};
      }
      const auto w = static_cast<std::uint32_t>(*meta->width);
      const auto h = static_cast<std::uint32_t>(*meta->height);
      if (col == FileListColumn::AspectRatio) {
        auto gcd = [](std::uint32_t a, std::uint32_t b) {
          while (b != 0) {
            const auto t = b;
            b = a % b;
            a = t;
          }
          return a;
        };
        const auto g = gcd(w, h);
        return QStringLiteral("%1:%2").arg(w / g).arg(h / g);
      }
      // Dimensions
      return QStringLiteral("%1×%2").arg(w).arg(h);
    }
    case FileListColumn::Duration: {
      if (fi->is_directory()) {
        return {};
      }
      const auto meta = filter::MediaMetaCache::instance().try_get(fi->path());
      if (!meta) {
        if (!fi->path().empty() && !filter::MediaMetaCache::instance().is_negative(fi->path())) {
          const int row = index.row();
          filter::MediaMetaCache::instance().request(
              fi->path(), 0,
              [this, row](const std::string&, std::optional<filter::MediaInfo>, std::uint64_t) {
                QMetaObject::invokeMethod(const_cast<FileListModel*>(this), "notify_row_changed",
                                          Qt::QueuedConnection, Q_ARG(int, row));
              });
        }
        return {};
      }
      if (!meta->duration_ms) {
        return {};
      }
      const auto total_s = static_cast<qint64>(*meta->duration_ms / 1000);
      const qint64 h = total_s / 3600;
      const qint64 m = (total_s % 3600) / 60;
      const qint64 s = total_s % 60;
      if (h > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(h)
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0'));
      }
      return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
    }
    case FileListColumn::Modified:
      return format_mtime(fi->mtime());
    case FileListColumn::Accessed:
      return fi->has_atime() ? format_sys_time(fi->atime()) : QString{};
    case FileListColumn::Changed:
      return fi->has_ctime() ? format_sys_time(fi->ctime()) : QString{};
    case FileListColumn::Birth:
      return fi->has_birthtime() ? format_sys_time(fi->birthtime()) : QString{};
    case FileListColumn::Type:
      return type_label(*fi);
    case FileListColumn::Count:
      break;
    }
  }

  if (role == Qt::TextAlignmentRole) {
    const auto col = static_cast<FileListColumn>(index.column());
    if (col == FileListColumn::Size || col == FileListColumn::Width || col == FileListColumn::Height
        || col == FileListColumn::AspectRatio || col == FileListColumn::Framerate
        || col == FileListColumn::Duration) {
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

  if (role == LaunchFlashRole) {
    return is_launch_flash(QString::fromStdString(fi->path().string()));
  }

  if (role == IsOpenedRole) {
    return is_opened(QString::fromStdString(fi->path().string()));
  }

  if (role == IsHiddenRole) {
    const std::string name = fi->basename();
    return !name.empty() && name.front() == '.';
  }

  if (role == IsSymlinkRole) {
    return fi->is_symlink();
  }

  // Dim hidden (dotfile) captions / row background when "Show Hidden" is on.
  if (role == Qt::ForegroundRole) {
    const std::string name = fi->basename();
    if (!name.empty() && name.front() == '.') {
      return QColor(120, 120, 120);
    }
  }
  if (role == Qt::BackgroundRole) {
    const std::string name = fi->basename();
    if (!name.empty() && name.front() == '.') {
      return QColor(200, 200, 210);
    }
  }

  if (role == ChildCountRole) {
    if (!fi->is_directory()) {
      return QVariant::fromValue(static_cast<qint64>(-1));
    }
    const QString path = QString::fromStdString(fi->path().string());
    const auto it = child_counts_.constFind(path);
    if (it == child_counts_.constEnd()) {
      // Kick off async count on first query (paint/delegate).
      const_cast<FileListModel*>(this)->request_child_count(path);
      return QVariant::fromValue(static_cast<qint64>(-1));
    }
    return QVariant::fromValue(it.value());
  }

  if (role == TimeGapSecondsRole) {
    if (!show_timegaps_ || index.row() <= 0) {
      return QVariant::fromValue(static_cast<qint64>(0));
    }
    const auto* prev = file_at(index.row() - 1);
    if (prev == nullptr) {
      return QVariant::fromValue(static_cast<qint64>(0));
    }
    try {
      const auto a = std::chrono::clock_cast<std::chrono::system_clock>(prev->mtime());
      const auto b = std::chrono::clock_cast<std::chrono::system_clock>(fi->mtime());
      const auto secs = std::chrono::duration_cast<std::chrono::seconds>(a - b).count();
      return QVariant::fromValue(static_cast<qint64>(std::llabs(secs)));
    } catch (...) {
      return QVariant::fromValue(static_cast<qint64>(0));
    }
  }

  if (role == AccessDeniedRole || role == IsUnreadableRole) {
    if (fi->is_synthetic() || fi->path().empty()) {
      return false;
    }
    // Effective access for the current process (more accurate than mode bits alone).
    return ::access(fi->path().c_str(), R_OK) != 0;
  }

  if (role == IsUnwritableRole) {
    if (fi->is_synthetic() || fi->path().empty()) {
      return false;
    }
    return ::access(fi->path().c_str(), W_OK) != 0;
  }

  if (role == GroupLabelRole) {
    if (collection_ == nullptr) {
      return {};
    }
    // Index-based cache — avoids localtime/path work on every paint/sizeHint.
    const auto label = collection_->group_label_at(static_cast<std::size_t>(index.row()));
    if (label.empty()) {
      return {};
    }
    return QString::fromStdString(label);
  }

  if (role == IsGroupStartRole) {
    if (collection_ == nullptr || collection_->group_mode() == collection::GroupMode::None) {
      return false;
    }
    return collection_->is_group_start_at(static_cast<std::size_t>(index.row()));
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
  case FileListColumn::Width:
    return QStringLiteral("Width");
  case FileListColumn::Height:
    return QStringLiteral("Height");
  case FileListColumn::Dimensions:
    return QStringLiteral("Dimensions");
  case FileListColumn::AspectRatio:
    return QStringLiteral("Aspect");
  case FileListColumn::Framerate:
    return QStringLiteral("FPS");
  case FileListColumn::Duration:
    return QStringLiteral("Duration");
  case FileListColumn::Modified:
    return QStringLiteral("Modified");
  case FileListColumn::Accessed:
    return QStringLiteral("Accessed");
  case FileListColumn::Changed:
    return QStringLiteral("Changed");
  case FileListColumn::Birth:
    return QStringLiteral("Created");
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
  QStringList location_urls;
  std::vector<int> rows;
  for (const QModelIndex& idx : indexes) {
    rows.push_back(idx.row());
  }
  std::sort(rows.begin(), rows.end());
  rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

  const auto drop_cache = archive_member_cache_root("dirtoo-archive-drop");

  for (int row : rows) {
    if (const fs::FileInfo* fi = file_at(row)) {
      if (fi->location().is_archive()) {
        location_urls.push_back(QString::fromStdString(fi->location().as_url()));
        // External apps need a real path. Prefer extract-to-cache so file://
        // URLs work outside dirtoo; same-app drop still sees Location URLs.
        if (!fi->location().entry_path().empty()) {
          const auto archive_file = fi->location().as_path();
          const auto member = fi->location().entry_path();
          const auto dest_dir = archive_member_dest_dir(drop_cache, archive_file);
          if (auto extracted =
                  ensure_archive_member_extracted(archive_file, member, dest_dir)) {
            urls.push_back(
                QUrl::fromLocalFile(QString::fromStdString(extracted->string())));
            continue;
          }
        } else {
          // Archive root entry → the archive file itself.
          urls.push_back(
              QUrl::fromLocalFile(QString::fromStdString(fi->location().as_path().string())));
          continue;
        }
        // Extract failed: fall back to Location URL only.
        urls.push_back(QUrl(QString::fromStdString(fi->location().as_url())));
      } else if (fi->is_synthetic()) {
        location_urls.push_back(QString::fromStdString(fi->location().as_url()));
        urls.push_back(QUrl(QString::fromStdString(fi->location().as_url())));
      } else {
        urls.push_back(QUrl::fromLocalFile(QString::fromStdString(fi->path().string())));
      }
    }
  }
  if (urls.isEmpty()) {
    return nullptr;
  }
  auto* mime = new QMimeData;
  mime->setUrls(urls);
  if (!location_urls.isEmpty()) {
    mime->setData(QStringLiteral("application/x-dirtoo-locations"),
                  location_urls.join(QLatin1Char('\n')).toUtf8());
  }
  return mime;
}

bool FileListModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                                    int column, const QModelIndex& parent) const
{
  (void)row;
  (void)column;
  (void)parent;
  (void)action; // Qt may probe with IgnoreAction; actual action is chosen in dropEvent.
  if (data == nullptr) {
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

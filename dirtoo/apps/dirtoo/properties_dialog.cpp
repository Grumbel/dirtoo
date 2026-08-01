// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "properties_dialog.hpp"
#include "size_format.hpp"

#include "dirtoo/filter/media_meta_cache.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QGroupBox>
#include <QLabel>
#include <QLocale>
#include <QMimeDatabase>
#include <QVBoxLayout>

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>

namespace dirtoo::app {
namespace {

QString format_epoch(std::time_t t)
{
  if (t <= 0) {
    return QStringLiteral("—");
  }
  const QDateTime dt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t));
  return QLocale::system().toString(dt, QLocale::LongFormat);
}

QString type_of(const fs::FileInfo& fi)
{
  if (fi.is_directory()) {
    return QStringLiteral("Folder");
  }
  if (fi.is_symlink()) {
    return QStringLiteral("Symbolic link");
  }
  return QStringLiteral("File");
}

QString user_name(uid_t uid)
{
  if (const auto* pw = getpwuid(uid)) {
    return QString::fromLocal8Bit(pw->pw_name);
  }
  return QString::number(uid);
}

QString group_name(gid_t gid)
{
  if (const auto* gr = getgrgid(gid)) {
    return QString::fromLocal8Bit(gr->gr_name);
  }
  return QString::number(gid);
}

QCheckBox* perm_box(const QString& label, bool on, QWidget* parent)
{
  auto* box = new QCheckBox(label, parent);
  box->setChecked(on);
  box->setEnabled(false); // display-only for now
  return box;
}

void add_permissions(QVBoxLayout* outer, mode_t mode, QWidget* parent)
{
  auto* box = new QGroupBox(QStringLiteral("Permissions"), parent);
  auto* grid = new QFormLayout(box);

  auto* user_row = new QHBoxLayout();
  user_row->addWidget(perm_box(QStringLiteral("Read"), mode & S_IRUSR, parent));
  user_row->addWidget(perm_box(QStringLiteral("Write"), mode & S_IWUSR, parent));
  user_row->addWidget(perm_box(QStringLiteral("Exec"), mode & S_IXUSR, parent));
  grid->addRow(QStringLiteral("Owner:"), user_row);

  auto* group_row = new QHBoxLayout();
  group_row->addWidget(perm_box(QStringLiteral("Read"), mode & S_IRGRP, parent));
  group_row->addWidget(perm_box(QStringLiteral("Write"), mode & S_IWGRP, parent));
  group_row->addWidget(perm_box(QStringLiteral("Exec"), mode & S_IXGRP, parent));
  grid->addRow(QStringLiteral("Group:"), group_row);

  auto* other_row = new QHBoxLayout();
  other_row->addWidget(perm_box(QStringLiteral("Read"), mode & S_IROTH, parent));
  other_row->addWidget(perm_box(QStringLiteral("Write"), mode & S_IWOTH, parent));
  other_row->addWidget(perm_box(QStringLiteral("Exec"), mode & S_IXOTH, parent));
  grid->addRow(QStringLiteral("Other:"), other_row);

  auto* special_row = new QHBoxLayout();
  special_row->addWidget(perm_box(QStringLiteral("Setuid"), mode & S_ISUID, parent));
  special_row->addWidget(perm_box(QStringLiteral("Setgid"), mode & S_ISGID, parent));
  special_row->addWidget(perm_box(QStringLiteral("Sticky"), mode & S_ISVTX, parent));
  grid->addRow(QStringLiteral("Special:"), special_row);

  const QString octal = QStringLiteral("%1").arg(mode & 07777, 4, 8, QLatin1Char('0'));
  grid->addRow(QStringLiteral("Mode:"), new QLabel(octal, parent));

  outer->addWidget(box);
}

} // namespace

void show_properties_dialog(QWidget* parent, const std::vector<fs::FileInfo>& items)
{
  if (items.empty()) {
    return;
  }

  QDialog dialog(parent);
  dialog.setWindowTitle(items.size() == 1 ? QStringLiteral("Properties")
                                          : QStringLiteral("Properties (%1 items)").arg(items.size()));
  dialog.setMinimumWidth(460);

  auto* layout = new QVBoxLayout(&dialog);

  if (items.size() == 1) {
    const auto& fi = items.front();
    const auto path = fi.path();

    auto* general = new QGroupBox(QStringLiteral("General"), &dialog);
    auto* form = new QFormLayout(general);
    auto* name_lbl = new QLabel(QString::fromStdString(fi.basename()), &dialog);
    name_lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(QStringLiteral("Name:"), name_lbl);
    auto* loc_lbl = new QLabel(QString::fromStdString(path.parent_path().string()), &dialog);
    loc_lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    loc_lbl->setWordWrap(true);
    form->addRow(QStringLiteral("Location:"), loc_lbl);
    form->addRow(QStringLiteral("Type:"), new QLabel(type_of(fi), &dialog));

    QMimeDatabase mime_db;
    const auto mime = mime_db.mimeTypeForFile(QString::fromStdString(path.string()));
    form->addRow(QStringLiteral("MIME type:"), new QLabel(mime.name(), &dialog));

    if (fi.is_regular_file() || fi.is_symlink()) {
      form->addRow(QStringLiteral("Size:"),
                   new QLabel(format_byte_size(fi.size()), &dialog));
    }
    auto* full_lbl = new QLabel(QString::fromStdString(path.string()), &dialog);
    full_lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    full_lbl->setWordWrap(true);
    form->addRow(QStringLiteral("Full path:"), full_lbl);
    layout->addWidget(general);

    struct ::stat st {};
    if (::stat(path.c_str(), &st) == 0) {
      auto* ownership = new QGroupBox(QStringLiteral("Ownership"), &dialog);
      auto* oform = new QFormLayout(ownership);
      oform->addRow(QStringLiteral("User:"), new QLabel(user_name(st.st_uid), &dialog));
      oform->addRow(QStringLiteral("Group:"), new QLabel(group_name(st.st_gid), &dialog));
      layout->addWidget(ownership);

      auto* times = new QGroupBox(QStringLiteral("Timestamps"), &dialog);
      auto* tform = new QFormLayout(times);
      tform->addRow(QStringLiteral("Accessed:"), new QLabel(format_epoch(st.st_atime), &dialog));
      tform->addRow(QStringLiteral("Modified:"), new QLabel(format_epoch(st.st_mtime), &dialog));
      tform->addRow(QStringLiteral("Changed:"), new QLabel(format_epoch(st.st_ctime), &dialog));
      layout->addWidget(times);

      add_permissions(layout, st.st_mode, &dialog);
    }

    // Media / document meta from process cache only (no GUI-thread probe).
    if (const auto meta = filter::MediaMetaCache::instance().try_get(path)) {
      auto* media = new QGroupBox(QStringLiteral("Media / document"), &dialog);
      auto* mform = new QFormLayout(media);
      bool any = false;
      if (meta->width && meta->height) {
        mform->addRow(QStringLiteral("Dimensions:"),
                      new QLabel(QStringLiteral("%1 × %2").arg(*meta->width).arg(*meta->height),
                                 &dialog));
        any = true;
      }
      if (meta->duration_ms && *meta->duration_ms > 0) {
        const auto ms = *meta->duration_ms;
        const int secs = static_cast<int>(ms / 1000);
        mform->addRow(QStringLiteral("Duration:"),
                      new QLabel(QStringLiteral("%1:%2:%3")
                                     .arg(secs / 3600, 2, 10, QLatin1Char('0'))
                                     .arg((secs % 3600) / 60, 2, 10, QLatin1Char('0'))
                                     .arg(secs % 60, 2, 10, QLatin1Char('0')),
                                 &dialog));
        any = true;
      }
      if (meta->framerate && *meta->framerate > 0.0) {
        mform->addRow(QStringLiteral("Frame rate:"),
                      new QLabel(QStringLiteral("%1 fps").arg(*meta->framerate, 0, 'g', 3),
                                 &dialog));
        any = true;
      }
      if (meta->pages && *meta->pages > 0) {
        mform->addRow(QStringLiteral("Pages:"),
                      new QLabel(QString::number(*meta->pages), &dialog));
        any = true;
      }
      if (meta->file_count && *meta->file_count > 0) {
        mform->addRow(QStringLiteral("Archive files:"),
                      new QLabel(QString::number(*meta->file_count), &dialog));
        any = true;
      }
      if (any) {
        layout->addWidget(media);
      } else {
        media->deleteLater();
      }
    }
  } else {
    std::uint64_t total_size = 0;
    int files = 0;
    int dirs = 0;
    for (const auto& fi : items) {
      if (fi.is_directory()) {
        ++dirs;
      } else {
        ++files;
        total_size += fi.size();
      }
    }
    auto* form = new QFormLayout();
    form->addRow(QStringLiteral("Items:"), new QLabel(QString::number(items.size()), &dialog));
    form->addRow(QStringLiteral("Files:"), new QLabel(QString::number(files), &dialog));
    form->addRow(QStringLiteral("Folders:"), new QLabel(QString::number(dirs), &dialog));
    form->addRow(QStringLiteral("Total size:"),
                 new QLabel(format_byte_size(total_size), &dialog));
    layout->addLayout(form);
  }

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  layout->addWidget(buttons);

  dialog.exec();
}

} // namespace dirtoo::app

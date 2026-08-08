// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "properties_dialog.hpp"
#include "size_format.hpp"
#include "operations_history.hpp"

#include "dirtoo/filter/media_meta_cache.hpp"
#include "dirops/ops.hpp"
#include <QFileInfo>
#include <QPixmap>
#include <QFileIconProvider>
#include "dirtoo/thumbnail/thumbnailer.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QGroupBox>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPushButton>
#include <QVBoxLayout>

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>

#include <array>

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


QPixmap properties_thumbnail(const fs::FileInfo& fi)
{
  if (fi.is_synthetic() || fi.path().empty()) {
    return {};
  }
  const auto loc = fi.location();
  // Prefer freedesktop large cache if already generated.
  const QString cached =
      thumbnail::Thumbnailer::cache_path_for(loc, QStringLiteral("large"));
  if (QFileInfo::exists(cached)) {
    QPixmap pm(cached);
    if (!pm.isNull()) {
      return pm.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
  }
  const QString normal =
      thumbnail::Thumbnailer::cache_path_for(loc, QStringLiteral("normal"));
  if (QFileInfo::exists(normal)) {
    QPixmap pm(normal);
    if (!pm.isNull()) {
      return pm.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
  }
  // Fallback: system file icon.
  static QFileIconProvider provider;
  const QIcon icon = provider.icon(QFileInfo(QString::fromStdString(fi.path().string())));
  return icon.pixmap(128, 128);
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

struct PermBits {
  QCheckBox* box = nullptr;
  mode_t bit = 0;
};

struct PermissionsEditor {
  std::array<PermBits, 12> bits{};
  QLabel* octal_label = nullptr;
  mode_t original = 0;

  [[nodiscard]] mode_t current_mode() const
  {
    mode_t m = 0;
    for (const auto& b : bits) {
      if (b.box != nullptr && b.box->isChecked()) {
        m |= b.bit;
      }
    }
    return m;
  }

  void refresh_octal()
  {
    if (octal_label != nullptr) {
      octal_label->setText(QStringLiteral("%1").arg(current_mode() & 07777, 4, 8, QLatin1Char('0')));
    }
  }
};

QCheckBox* make_perm_box(const QString& label, bool on, bool editable, QWidget* parent)
{
  auto* box = new QCheckBox(label, parent);
  box->setChecked(on);
  box->setEnabled(editable);
  return box;
}

PermissionsEditor* add_permissions(QVBoxLayout* outer, mode_t mode, bool editable, QWidget* parent)
{
  auto* editor = new PermissionsEditor;
  editor->original = mode & 07777;

  auto* box = new QGroupBox(QStringLiteral("Permissions"), parent);
  auto* grid = new QFormLayout(box);

  auto add_row = [&](const QString& title, mode_t r, mode_t w, mode_t x, int base) {
    auto* row = new QHBoxLayout();
    auto* br = make_perm_box(QStringLiteral("Read"), mode & r, editable, parent);
    auto* bw = make_perm_box(QStringLiteral("Write"), mode & w, editable, parent);
    auto* bx = make_perm_box(QStringLiteral("Exec"), mode & x, editable, parent);
    editor->bits[static_cast<std::size_t>(base)] = {br, r};
    editor->bits[static_cast<std::size_t>(base + 1)] = {bw, w};
    editor->bits[static_cast<std::size_t>(base + 2)] = {bx, x};
    row->addWidget(br);
    row->addWidget(bw);
    row->addWidget(bx);
    grid->addRow(title, row);
  };

  add_row(QStringLiteral("Owner:"), S_IRUSR, S_IWUSR, S_IXUSR, 0);
  add_row(QStringLiteral("Group:"), S_IRGRP, S_IWGRP, S_IXGRP, 3);
  add_row(QStringLiteral("Other:"), S_IROTH, S_IWOTH, S_IXOTH, 6);

  auto* special_row = new QHBoxLayout();
  auto* su = make_perm_box(QStringLiteral("Setuid"), mode & S_ISUID, editable, parent);
  auto* sg = make_perm_box(QStringLiteral("Setgid"), mode & S_ISGID, editable, parent);
  auto* st = make_perm_box(QStringLiteral("Sticky"), mode & S_ISVTX, editable, parent);
  editor->bits[9] = {su, S_ISUID};
  editor->bits[10] = {sg, S_ISGID};
  editor->bits[11] = {st, S_ISVTX};
  special_row->addWidget(su);
  special_row->addWidget(sg);
  special_row->addWidget(st);
  grid->addRow(QStringLiteral("Special:"), special_row);

  editor->octal_label =
      new QLabel(QStringLiteral("%1").arg(mode & 07777, 4, 8, QLatin1Char('0')), parent);
  grid->addRow(QStringLiteral("Mode:"), editor->octal_label);

  if (editable) {
    for (const auto& b : editor->bits) {
      if (b.box != nullptr) {
        QObject::connect(b.box, &QCheckBox::toggled, box, [editor](bool) { editor->refresh_octal(); });
      }
    }
  }

  outer->addWidget(box);
  return editor;
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
  PermissionsEditor* perm_editor = nullptr;
  std::filesystem::path chmod_path;
  bool can_edit_perms = false;

  if (items.size() == 1) {
    const auto& fi = items.front();
    const auto path = fi.path();

    // Archive / synthetic entries have no real mode to chmod.
    can_edit_perms = !fi.is_synthetic() && !fi.location().is_archive();
    chmod_path = path;

    auto* general = new QGroupBox(QStringLiteral("General"), &dialog);
    auto* form = new QFormLayout(general);

    {
      const QPixmap thumb = properties_thumbnail(fi);
      auto* thumb_lbl = new QLabel(&dialog);
      thumb_lbl->setAlignment(Qt::AlignCenter);
      thumb_lbl->setMinimumSize(128, 128);
      thumb_lbl->setMaximumSize(160, 160);
      if (!thumb.isNull()) {
        thumb_lbl->setPixmap(thumb);
      } else {
        thumb_lbl->setText(QStringLiteral("(no preview)"));
      }
      form->addRow(QStringLiteral("Preview:"), thumb_lbl);
    }

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

      perm_editor = add_permissions(layout, st.st_mode, can_edit_perms, &dialog);
    } else {
      can_edit_perms = false;
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

  QDialogButtonBox* buttons = nullptr;
  if (can_edit_perms && perm_editor != nullptr) {
    // GNOME order: Cancel left, OK right (style hint installs GnomeLayout).
    buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
      const mode_t new_mode = perm_editor->current_mode();
      if (new_mode == perm_editor->original) {
        dialog.accept();
        return;
      }
      auto result = dirops::set_permissions(chmod_path, static_cast<std::uint32_t>(new_mode));
      if (!result) {
        operations_history().record_simple(
            OperationKind::Permissions, {chmod_path}, chmod_path, false,
            QString::fromStdString(result.error().to_string()));
        QMessageBox::warning(&dialog, QStringLiteral("Permissions"),
                             QString::fromStdString(result.error().to_string()));
        return;
      }
      operations_history().record_simple(OperationKind::Permissions, {chmod_path}, chmod_path,
                                         true);
      dialog.accept();
    });
  } else {
    buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  }
  layout->addWidget(buttons);

  dialog.exec();
  delete perm_editor;
}

} // namespace dirtoo::app

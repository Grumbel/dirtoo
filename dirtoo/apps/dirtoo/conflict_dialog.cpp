// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "conflict_dialog.hpp"

#include <filesystem>
#include "size_format.hpp"

#include "dirtoo/fs/location.hpp"
#include "dirtoo/thumbnail/thumbnailer.hpp"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include <chrono>
#include <ctime>

namespace dirtoo::app {
namespace {

QString format_size(std::uint64_t bytes)
{
  return format_byte_size(bytes);
}

QString format_mtime(const std::filesystem::file_time_type& ftp)
{
  try {
    const auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftp);
    const auto secs = std::chrono::system_clock::to_time_t(sctp);
    std::tm tm{};
    localtime_r(&secs, &tm);
    char buf[64];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm) > 0) {
      return QString::fromUtf8(buf);
    }
  } catch (...) {
  }
  return QStringLiteral("—");
}

/// Prefer freedesktop thumbnail cache; fall back to theme/file icon.
QPixmap thumbnail_for(const std::filesystem::path& path, int edge = 96)
{
  if (path.empty()) {
    return {};
  }
  try {
    const auto loc = dirtoo::fs::Location::from_path(path);
    const QString cached =
        dirtoo::thumbnail::Thumbnailer::cache_path_for(loc, QStringLiteral("normal"));
    QPixmap pm(cached);
    if (!pm.isNull()) {
      return pm.scaled(edge, edge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    const QString large =
        dirtoo::thumbnail::Thumbnailer::cache_path_for(loc, QStringLiteral("large"));
    pm = QPixmap(large);
    if (!pm.isNull()) {
      return pm.scaled(edge, edge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
  } catch (...) {
  }
  QFileIconProvider provider;
  const QIcon icon = provider.icon(QFileInfo(QString::fromStdString(path.string())));
  return icon.pixmap(edge, edge);
}

void fill_file_info(QFormLayout* form, const std::filesystem::path& path, const QString& fallback_name,
                    QLabel* thumb_label)
{
  QString name = fallback_name;
  QString size = QStringLiteral("—");
  QString mtime = QStringLiteral("—");
  if (!path.empty()) {
    name = QString::fromStdString(path.filename().string());
    std::error_code ec;
    if (std::filesystem::exists(path, ec) && !ec) {
      const auto sz = std::filesystem::file_size(path, ec);
      if (!ec) {
        size = format_size(static_cast<std::uint64_t>(sz));
      }
      const auto mt = std::filesystem::last_write_time(path, ec);
      if (!ec) {
        mtime = format_mtime(mt);
      }
    }
  }
  if (thumb_label != nullptr) {
    const QPixmap pm = thumbnail_for(path);
    if (!pm.isNull()) {
      thumb_label->setPixmap(pm);
    } else {
      thumb_label->setText(QStringLiteral("—"));
    }
    thumb_label->setAlignment(Qt::AlignCenter);
    thumb_label->setFixedSize(104, 104);
    thumb_label->setStyleSheet(QStringLiteral("QLabel { background: palette(base); border: 1px solid palette(mid); }"));
  }
  form->addRow(QStringLiteral("Name:"), new QLabel(name));
  form->addRow(QStringLiteral("Size:"), new QLabel(size));
  form->addRow(QStringLiteral("Modified:"), new QLabel(mtime));
}

} // namespace

std::optional<ConflictDecision> ask_conflict_policy(QWidget* parent,
                                                    const QString& destination_name,
                                                    const std::filesystem::path& source_path,
                                                    const std::filesystem::path& destination_path)
{
  QDialog dialog(parent);
  dialog.setWindowTitle(QStringLiteral("Confirm to replace files"));
  dialog.setModal(true);
  dialog.setMinimumWidth(520);

  auto* layout = new QVBoxLayout(&dialog);

  std::error_code dest_ec;
  const bool dest_is_dir =
      !destination_path.empty()
      && std::filesystem::is_directory(destination_path, dest_ec)
      && !dest_ec;

  // Header must not call a folder a "file" — Replace is disabled for directories.
  auto* header = new QLabel(&dialog);
  if (dest_is_dir) {
    header->setText(
        QStringLiteral("<big>This folder already contains a <b>folder</b> named <b>%1</b></big>")
            .arg(destination_name.toHtmlEscaped()));
  } else {
    header->setText(
        QStringLiteral("<big>This folder already contains a file named <b>%1</b></big>")
            .arg(destination_name.toHtmlEscaped()));
  }
  header->setTextFormat(Qt::RichText);
  header->setWordWrap(true);
  layout->addWidget(header);

  auto* question = new QLabel(&dialog);
  if (dest_is_dir) {
    question->setText(
        QStringLiteral(
            "The destination is a <b>folder</b>. Replacing it would delete the entire "
            "folder tree, which is not offered. Choose <b>Rename</b> (keep both) or "
            "<b>Skip</b>."));
    question->setTextFormat(Qt::RichText);
  } else {
    question->setText(QStringLiteral("Replace the existing file in the destination folder?"));
  }
  question->setWordWrap(true);
  layout->addWidget(question);

  auto* source_box = new QGroupBox(QStringLiteral("New / Source:"), &dialog);
  auto* source_row = new QHBoxLayout(source_box);
  auto* source_thumb = new QLabel(source_box);
  auto* source_form = new QFormLayout();
  fill_file_info(source_form, source_path, destination_name, source_thumb);
  source_row->addWidget(source_thumb);
  source_row->addLayout(source_form, 1);
  layout->addWidget(source_box);

  auto* dest_box = new QGroupBox(QStringLiteral("Existing / Destination:"), &dialog);
  auto* dest_row = new QHBoxLayout(dest_box);
  auto* dest_thumb = new QLabel(dest_box);
  auto* dest_form = new QFormLayout();
  fill_file_info(dest_form, destination_path, destination_name, dest_thumb);
  dest_row->addWidget(dest_thumb);
  dest_row->addLayout(dest_form, 1);
  layout->addWidget(dest_box);

  auto* apply_all = new QCheckBox(QStringLiteral("Repeat action for all files"), &dialog);
  layout->addWidget(apply_all);

  auto* buttons = new QDialogButtonBox(&dialog);
  auto* overwrite = buttons->addButton(QStringLiteral("Replace"), QDialogButtonBox::YesRole);
  auto* rename = buttons->addButton(QStringLiteral("Rename"), QDialogButtonBox::ActionRole);
  auto* skip = buttons->addButton(QStringLiteral("Skip"), QDialogButtonBox::NoRole);
  buttons->addButton(QDialogButtonBox::Cancel);
  if (dest_is_dir) {
    overwrite->setEnabled(false);
    overwrite->setToolTip(
        QStringLiteral("Refusing to replace a folder (would delete the whole tree)"));
    rename->setDefault(true);
  } else {
    overwrite->setDefault(true);
  }
  layout->addWidget(buttons);

  std::optional<ConflictDecision> chosen;

  QObject::connect(overwrite, &QPushButton::clicked, &dialog, [&] {
    chosen = ConflictDecision{dirops::ConflictPolicy::Overwrite, apply_all->isChecked()};
    dialog.accept();
  });
  QObject::connect(rename, &QPushButton::clicked, &dialog, [&] {
    chosen = ConflictDecision{dirops::ConflictPolicy::Rename, apply_all->isChecked()};
    dialog.accept();
  });
  QObject::connect(skip, &QPushButton::clicked, &dialog, [&] {
    chosen = ConflictDecision{dirops::ConflictPolicy::Skip, apply_all->isChecked()};
    dialog.accept();
  });
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted) {
    return std::nullopt;
  }
  return chosen;
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "properties_dialog.hpp"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QVBoxLayout>

#include <sys/stat.h>

namespace dirtoo::app {
namespace {

QString format_mtime(const std::filesystem::path& path)
{
  struct ::stat st {};
  if (::stat(path.c_str(), &st) != 0) {
    return QStringLiteral("—");
  }
  const QDateTime dt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(st.st_mtime));
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

} // namespace

void show_properties_dialog(QWidget* parent, const std::vector<fs::FileInfo>& items)
{
  if (items.empty()) {
    return;
  }

  QDialog dialog(parent);
  dialog.setWindowTitle(items.size() == 1 ? QStringLiteral("Properties")
                                          : QStringLiteral("Properties (%1 items)").arg(items.size()));
  dialog.setMinimumWidth(420);

  auto* layout = new QVBoxLayout(&dialog);
  auto* form = new QFormLayout();

  if (items.size() == 1) {
    const auto& fi = items.front();
    form->addRow(QStringLiteral("Name:"), new QLabel(QString::fromStdString(fi.basename())));
    form->addRow(QStringLiteral("Location:"),
                 new QLabel(QString::fromStdString(fi.path().parent_path().string())));
    form->addRow(QStringLiteral("Type:"), new QLabel(type_of(fi)));
    if (fi.is_regular_file()) {
      form->addRow(QStringLiteral("Size:"),
                   new QLabel(QLocale::system().formattedDataSize(static_cast<qint64>(fi.size()))));
    }
    form->addRow(QStringLiteral("Modified:"), new QLabel(format_mtime(fi.path())));
    form->addRow(QStringLiteral("Full path:"),
                 new QLabel(QString::fromStdString(fi.path().string())));
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
    form->addRow(QStringLiteral("Items:"), new QLabel(QString::number(items.size())));
    form->addRow(QStringLiteral("Files:"), new QLabel(QString::number(files)));
    form->addRow(QStringLiteral("Folders:"), new QLabel(QString::number(dirs)));
    form->addRow(QStringLiteral("Total size:"),
                 new QLabel(QLocale::system().formattedDataSize(static_cast<qint64>(total_size))));
  }

  layout->addLayout(form);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  layout->addWidget(buttons);
  dialog.exec();
}

} // namespace dirtoo::app

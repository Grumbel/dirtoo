// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "conflict_dialog.hpp"
#include "size_format.hpp"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLocale>
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

void fill_file_info(QFormLayout* form, const std::filesystem::path& path, const QString& fallback_name)
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
  dialog.setMinimumWidth(420);

  auto* layout = new QVBoxLayout(&dialog);

  auto* header = new QLabel(
      QStringLiteral("<big>This folder already contains a file named <b>%1</b></big>")
          .arg(destination_name.toHtmlEscaped()),
      &dialog);
  header->setTextFormat(Qt::RichText);
  header->setWordWrap(true);
  layout->addWidget(header);

  layout->addWidget(new QLabel(QStringLiteral("Replace the existing file in the destination folder?"),
                               &dialog));

  auto* source_box = new QGroupBox(QStringLiteral("New / Source:"), &dialog);
  auto* source_form = new QFormLayout(source_box);
  fill_file_info(source_form, source_path, destination_name);
  layout->addWidget(source_box);

  auto* dest_box = new QGroupBox(QStringLiteral("Existing / Destination:"), &dialog);
  auto* dest_form = new QFormLayout(dest_box);
  fill_file_info(dest_form, destination_path, destination_name);
  layout->addWidget(dest_box);

  auto* apply_all = new QCheckBox(QStringLiteral("Repeat action for all files"), &dialog);
  layout->addWidget(apply_all);

  auto* buttons = new QDialogButtonBox(&dialog);
  auto* overwrite = buttons->addButton(QStringLiteral("Replace"), QDialogButtonBox::YesRole);
  auto* rename = buttons->addButton(QStringLiteral("Rename"), QDialogButtonBox::ActionRole);
  auto* skip = buttons->addButton(QStringLiteral("Skip"), QDialogButtonBox::NoRole);
  buttons->addButton(QDialogButtonBox::Cancel);
  overwrite->setDefault(true);
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

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "conflict_dialog.hpp"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace dirtoo::app {

std::optional<dirops::ConflictPolicy> ask_conflict_policy(QWidget* parent,
                                                          const QString& destination_name)
{
  QDialog dialog(parent);
  dialog.setWindowTitle(QStringLiteral("File exists"));
  dialog.setModal(true);

  auto* layout = new QVBoxLayout(&dialog);
  layout->addWidget(new QLabel(
      QStringLiteral("“%1” already exists. What should be done?").arg(destination_name),
      &dialog));

  auto* buttons = new QDialogButtonBox(&dialog);
  auto* overwrite = buttons->addButton(QStringLiteral("Overwrite"), QDialogButtonBox::AcceptRole);
  auto* rename = buttons->addButton(QStringLiteral("Rename"), QDialogButtonBox::AcceptRole);
  auto* skip = buttons->addButton(QStringLiteral("Skip"), QDialogButtonBox::AcceptRole);
  buttons->addButton(QDialogButtonBox::Cancel);
  layout->addWidget(buttons);

  std::optional<dirops::ConflictPolicy> chosen;

  QObject::connect(overwrite, &QPushButton::clicked, &dialog, [&] {
    chosen = dirops::ConflictPolicy::Overwrite;
    dialog.accept();
  });
  QObject::connect(rename, &QPushButton::clicked, &dialog, [&] {
    chosen = dirops::ConflictPolicy::Rename;
    dialog.accept();
  });
  QObject::connect(skip, &QPushButton::clicked, &dialog, [&] {
    chosen = dirops::ConflictPolicy::Skip;
    dialog.accept();
  });
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted) {
    return std::nullopt;
  }
  return chosen;
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "name_input_dialog.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace dirtoo::app {

std::optional<QString> ask_item_name(QWidget* parent, const QString& title, const QString& label,
                                     const QString& initial, const QString& accept_button)
{
  QDialog dialog(parent);
  dialog.setWindowTitle(title);
  dialog.setMinimumWidth(360);
  dialog.setModal(true);

  auto* layout = new QVBoxLayout(&dialog);
  layout->addWidget(new QLabel(label, &dialog));

  auto* edit = new QLineEdit(initial, &dialog);
  edit->selectAll();
  layout->addWidget(edit);

  auto* buttons = new QDialogButtonBox(&dialog);
  auto* ok = buttons->addButton(accept_button, QDialogButtonBox::AcceptRole);
  buttons->addButton(QDialogButtonBox::Cancel);
  layout->addWidget(buttons);

  auto update_ok = [edit, ok] {
    const QString t = edit->text();
    ok->setEnabled(!t.isEmpty() && !t.contains(QLatin1Char('/')));
  };
  update_ok();
  QObject::connect(edit, &QLineEdit::textChanged, &dialog, update_ok);
  QObject::connect(edit, &QLineEdit::returnPressed, &dialog, [ok, &dialog] {
    if (ok->isEnabled()) {
      dialog.accept();
    }
  });
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  QObject::connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);

  edit->setFocus();
  if (dialog.exec() != QDialog::Accepted) {
    return std::nullopt;
  }
  return edit->text().trimmed();
}

} // namespace dirtoo::app

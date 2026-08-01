// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "preferences_dialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>

namespace dirtoo::app {

bool show_preferences_dialog(QWidget* parent, AppSettings* settings)
{
  if (settings == nullptr) {
    return false;
  }

  QDialog dialog(parent);
  dialog.setWindowTitle(QStringLiteral("Preferences"));
  dialog.setMinimumWidth(360);

  auto* form = new QFormLayout();

  auto* view = new QComboBox(&dialog);
  view->addItem(QStringLiteral("Detail"), QStringLiteral("detail"));
  view->addItem(QStringLiteral("Icons"), QStringLiteral("icons"));
  view->setCurrentIndex(settings->view_mode == QLatin1String("icons") ? 1 : 0);

  auto* zoom = new QSpinBox(&dialog);
  zoom->setRange(0, 4);
  zoom->setValue(settings->zoom_index);
  zoom->setToolTip(QStringLiteral("Icon zoom level index (0–4)"));

  auto* hidden = new QCheckBox(QStringLiteral("Show hidden files"), &dialog);
  hidden->setChecked(settings->show_hidden);

  auto* show_filter = new QCheckBox(QStringLiteral("Show filter bar"), &dialog);
  show_filter->setChecked(settings->show_filter);

  auto* pin_filter = new QCheckBox(QStringLiteral("Keep filter bar visible (pin)"), &dialog);
  pin_filter->setChecked(settings->filter_pinned);

  form->addRow(QStringLiteral("Default view:"), view);
  form->addRow(QStringLiteral("Zoom level:"), zoom);
  form->addRow(hidden);
  form->addRow(show_filter);
  form->addRow(pin_filter);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  auto* layout = new QVBoxLayout(&dialog);
  layout->addLayout(form);
  layout->addWidget(buttons);

  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }

  settings->view_mode = view->currentData().toString();
  settings->zoom_index = zoom->value();
  settings->show_hidden = hidden->isChecked();
  settings->show_filter = show_filter->isChecked();
  settings->filter_pinned = pin_filter->isChecked();
  return true;
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "preferences_dialog.hpp"

#include "checksum_dialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
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
  dialog.setMinimumWidth(420);

  auto* form = new QFormLayout();

  auto* view = new QComboBox(&dialog);
  view->addItem(QStringLiteral("Detail"), QStringLiteral("detail"));
  view->addItem(QStringLiteral("Icons"), QStringLiteral("icons"));
  view->addItem(QStringLiteral("List"), QStringLiteral("small"));
  if (settings->view_mode == QLatin1String("icons")) {
    view->setCurrentIndex(1);
  } else if (settings->view_mode == QLatin1String("small")
             || settings->view_mode == QLatin1String("smallicons")) {
    view->setCurrentIndex(2);
  } else {
    view->setCurrentIndex(0);
  }

  auto* zoom = new QSpinBox(&dialog);
  zoom->setRange(0, 9);
  zoom->setValue(settings->zoom_index);
  zoom->setToolTip(QStringLiteral("Icon zoom level index (0–9 for grid; 0–6 for List view)"));

  auto* icon_detail = new QSpinBox(&dialog);
  icon_detail->setRange(0, 4);
  icon_detail->setValue(settings->icon_detail_level);
  icon_detail->setToolTip(QStringLiteral("0=none … 4=name+size+date under icons"));

  auto* group = new QComboBox(&dialog);
  group->addItem(QStringLiteral("None"), QStringLiteral("none"));
  group->addItem(QStringLiteral("Day"), QStringLiteral("day"));
  group->addItem(QStringLiteral("Directory"), QStringLiteral("directory"));
  group->addItem(QStringLiteral("Duration"), QStringLiteral("duration"));
  {
    const QString gm = settings->group_mode.toLower();
    int gi = 0;
    if (gm == QLatin1String("day")) {
      gi = 1;
    } else if (gm == QLatin1String("directory")) {
      gi = 2;
    } else if (gm == QLatin1String("duration")) {
      gi = 3;
    }
    group->setCurrentIndex(gi);
  }

  auto* crop = new QCheckBox(QStringLiteral("Crop thumbnails (cover instead of letterbox)"), &dialog);
  crop->setChecked(settings->crop_thumbnails);

  auto* dirs_first = new QCheckBox(QStringLiteral("Directories first when sorting"), &dialog);
  dirs_first->setChecked(settings->directories_first);

  auto* hidden = new QCheckBox(QStringLiteral("Show hidden files"), &dialog);
  hidden->setChecked(settings->show_hidden);

  auto* show_filter = new QCheckBox(QStringLiteral("Show filter bar"), &dialog);
  show_filter->setChecked(settings->show_filter);

  auto* pin_filter = new QCheckBox(QStringLiteral("Keep filter bar visible (pin)"), &dialog);
  pin_filter->setChecked(settings->filter_pinned);

  auto* size_units = new QComboBox(&dialog);
  size_units->addItem(QStringLiteral("Decimal (KB, MB, GB — base 1000)"), QStringLiteral("si"));
  size_units->addItem(QStringLiteral("Binary (KiB, MiB, GiB — base 1024)"), QStringLiteral("iec"));
  {
    const QString su = settings->size_units.toLower();
    size_units->setCurrentIndex(su == QLatin1String("iec") || su == QLatin1String("binary")
                                        || su == QLatin1String("mib")
                                    ? 1
                                    : 0);
  }
  size_units->setToolTip(
      QStringLiteral("How file sizes are shown in the list, properties, and transfers"));

  form->addRow(QStringLiteral("Default view:"), view);
  form->addRow(QStringLiteral("Zoom level:"), zoom);
  form->addRow(QStringLiteral("Icon caption detail:"), icon_detail);
  form->addRow(QStringLiteral("Group by:"), group);
  form->addRow(QStringLiteral("Size units:"), size_units);
  form->addRow(crop);
  form->addRow(dirs_first);
  form->addRow(hidden);
  form->addRow(show_filter);
  form->addRow(pin_filter);

  auto* checksums_btn = new QPushButton(QStringLiteral("Checksums…"), &dialog);
  checksums_btn->setToolTip(QStringLiteral("Compute or inspect cached file digests (CRC32/MD5/SHA-1/SHA-256)"));
  QObject::connect(checksums_btn, &QPushButton::clicked, &dialog, [&dialog] {
    show_checksum_dialog(&dialog, QStringList{});
  });

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  auto* layout = new QVBoxLayout(&dialog);
  layout->addLayout(form);
  layout->addWidget(checksums_btn);
  layout->addWidget(buttons);

  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }

  settings->view_mode = view->currentData().toString();
  settings->zoom_index = zoom->value();
  settings->icon_detail_level = icon_detail->value();
  settings->group_mode = group->currentData().toString();
  settings->size_units = size_units->currentData().toString();
  settings->crop_thumbnails = crop->isChecked();
  settings->directories_first = dirs_first->isChecked();
  settings->show_hidden = hidden->isChecked();
  settings->show_filter = show_filter->isChecked();
  settings->filter_pinned = pin_filter->isChecked();
  return true;
}

} // namespace dirtoo::app

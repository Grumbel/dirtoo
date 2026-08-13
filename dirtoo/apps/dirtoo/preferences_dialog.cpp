// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "preferences_dialog.hpp"

#include "checksum_dialog.hpp"

#include <QCheckBox>
#include <algorithm>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QWidget>

namespace dirtoo::app {
namespace {

QWidget* wrap_form(QFormLayout* form, QWidget* parent)
{
  auto* page = new QWidget(parent);
  auto* outer = new QVBoxLayout(page);
  outer->setContentsMargins(8, 8, 8, 8);
  outer->addLayout(form);
  outer->addStretch(1);
  return page;
}

} // namespace

bool show_preferences_dialog(QWidget* parent, AppSettings* settings)
{
  if (settings == nullptr) {
    return false;
  }

  QDialog dialog(parent);
  dialog.setWindowTitle(QStringLiteral("Preferences"));
  dialog.setMinimumSize(520, 420);
  dialog.resize(560, 480);

  auto* tabs = new QTabWidget(&dialog);

  // ----- Appearance -----
  auto* appearance_form = new QFormLayout();
  appearance_form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
  appearance_form->setHorizontalSpacing(12);
  appearance_form->setVerticalSpacing(10);

  auto* view = new QComboBox(&dialog);
  view->addItem(QStringLiteral("Detail"), QStringLiteral("detail"));
  view->addItem(QStringLiteral("Icons"), QStringLiteral("icons"));
  view->addItem(QStringLiteral("Small icons (List)"), QStringLiteral("small"));
  if (settings->view_mode == QLatin1String("icons")) {
    view->setCurrentIndex(1);
  } else if (settings->view_mode == QLatin1String("small")
             || settings->view_mode == QLatin1String("smallicons")) {
    view->setCurrentIndex(2);
  } else {
    view->setCurrentIndex(0);
  }
  view->setToolTip(QStringLiteral("View mode used when dirtoo starts"));

  auto* zoom_icons = new QSpinBox(&dialog);
  zoom_icons->setRange(0, 9);
  zoom_icons->setValue(settings->zoom_icons);
  zoom_icons->setToolTip(QStringLiteral("Thumbnail size index for Icons mode (0 = smallest)"));

  auto* zoom_list = new QSpinBox(&dialog);
  zoom_list->setRange(0, 6);
  zoom_list->setValue(settings->zoom_list);
  zoom_list->setToolTip(QStringLiteral("Icon size index for Small icons / List mode"));

  auto* zoom_detail = new QSpinBox(&dialog);
  zoom_detail->setRange(0, 6);
  zoom_detail->setValue(settings->zoom_detail);
  zoom_detail->setToolTip(QStringLiteral("Row icon size index for Detail mode"));

  auto* icon_detail = new QSpinBox(&dialog);
  icon_detail->setRange(0, 4);
  icon_detail->setValue(settings->icon_detail_level);
  icon_detail->setToolTip(
      QStringLiteral("Caption under icons: 0 = none, 1 = name, … 4 = name + size + date"));

  auto* group = new QComboBox(&dialog);
  group->addItem(QStringLiteral("None"), QStringLiteral("none"));
  group->addItem(QStringLiteral("Day"), QStringLiteral("day"));
  group->addItem(QStringLiteral("Directory"), QStringLiteral("directory"));
  group->addItem(QStringLiteral("Duration"), QStringLiteral("duration"));
  group->addItem(QStringLiteral("Session (time gaps)"), QStringLiteral("session"));
  {
    const QString gm = settings->group_mode.toLower();
    int gi = 0;
    if (gm == QLatin1String("day")) {
      gi = 1;
    } else if (gm == QLatin1String("directory")) {
      gi = 2;
    } else if (gm == QLatin1String("duration")) {
      gi = 3;
    } else if (gm == QLatin1String("session")) {
      gi = 4;
    }
    group->setCurrentIndex(gi);
  }

  auto* size_units = new QComboBox(&dialog);
  size_units->addItem(QStringLiteral("Decimal (kB, MB, GB — base 1000)"), QStringLiteral("si"));
  size_units->addItem(QStringLiteral("Binary (KiB, MiB, GiB — base 1024)"), QStringLiteral("iec"));
  {
    const QString su = settings->size_units.toLower();
    size_units->setCurrentIndex(su == QLatin1String("iec") || su == QLatin1String("binary")
                                        || su == QLatin1String("mib")
                                    ? 1
                                    : 0);
  }

  auto* crop = new QCheckBox(QStringLiteral("Crop thumbnails (fill tile instead of letterbox)"),
                             &dialog);
  crop->setChecked(settings->crop_thumbnails);

  auto* icon_spacing = new QSpinBox(&dialog);
  icon_spacing->setRange(0, 48);
  icon_spacing->setValue(std::clamp(settings->icon_spacing, 0, 48));
  icon_spacing->setSuffix(QStringLiteral(" px"));
  icon_spacing->setToolTip(QStringLiteral("Gap between tiles in Icons view."));

  auto* icon_pad = new QSpinBox(&dialog);
  icon_pad->setRange(16, 240);
  icon_pad->setValue(std::clamp(settings->icon_cell_padding, 16, 240));
  icon_pad->setSuffix(QStringLiteral(" px"));
  icon_pad->setToolTip(
      QStringLiteral("Extra width past the thumbnail for the file name caption. "
                     "Increase if names are often cropped."));

  auto* dirs_first = new QCheckBox(QStringLiteral("Directories first when sorting"), &dialog);
  dirs_first->setChecked(settings->directories_first);

  auto* default_sort = new QComboBox(&dialog);
  default_sort->setToolTip(
      QStringLiteral("Sort mode applied at startup and when Preferences are saved."));
  struct SortChoice {
    const char* label;
    const char* key;
  };
  static constexpr SortChoice kSortChoices[] = {
      {"Name", "name"},
      {"Size", "size"},
      {"Extension", "extension"},
      {"Date (modified)", "modified"},
      {"Type", "type"},
      {"Width", "width"},
      {"Height", "height"},
      {"Resolution", "resolution"},
      {"Aspect ratio", "aspectratio"},
      {"Duration", "duration"},
      {"Framerate", "framerate"},
      {"Permissions", "permissions"},
      {"Random", "random"},
  };
  for (const auto& sc : kSortChoices) {
    default_sort->addItem(QString::fromUtf8(sc.label), QString::fromUtf8(sc.key));
  }
  {
    const QString sk = settings->sort_key.toLower();
    int idx = 0;
    for (int i = 0; i < default_sort->count(); ++i) {
      if (default_sort->itemData(i).toString() == sk) {
        idx = i;
        break;
      }
    }
    default_sort->setCurrentIndex(idx);
  }

  auto* sort_ascending = new QCheckBox(QStringLiteral("Sort ascending (uncheck for reverse)"), &dialog);
  sort_ascending->setChecked(settings->sort_ascending);

  appearance_form->addRow(QStringLiteral("Default view:"), view);
  appearance_form->addRow(QStringLiteral("Icons zoom:"), zoom_icons);
  appearance_form->addRow(QStringLiteral("List zoom:"), zoom_list);
  appearance_form->addRow(QStringLiteral("Detail zoom:"), zoom_detail);
  appearance_form->addRow(QStringLiteral("Icon captions:"), icon_detail);
  appearance_form->addRow(QStringLiteral("Group by:"), group);
  appearance_form->addRow(QStringLiteral("Size units:"), size_units);
  appearance_form->addRow(crop);
  appearance_form->addRow(QStringLiteral("Icon spacing:"), icon_spacing);
  appearance_form->addRow(QStringLiteral("Icon label width:"), icon_pad);
  appearance_form->addRow(QStringLiteral("Default sort:"), default_sort);
  appearance_form->addRow(sort_ascending);
  appearance_form->addRow(dirs_first);
  tabs->addTab(wrap_form(appearance_form, &dialog), QStringLiteral("Appearance"));

  // ----- Interface -----
  auto* interface_form = new QFormLayout();
  interface_form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
  interface_form->setHorizontalSpacing(12);
  interface_form->setVerticalSpacing(10);

  auto* show_filter = new QCheckBox(QStringLiteral("Show filter bar at startup"), &dialog);
  show_filter->setChecked(settings->show_filter);
  show_filter->setToolTip(
      QStringLiteral("When off, the filter bar stays hidden until you show it "
                     "(View menu) or pin it. Filtering still works once visible."));

  auto* pin_filter = new QCheckBox(QStringLiteral("Pin filter bar (always visible)"), &dialog);
  pin_filter->setChecked(settings->filter_pinned);
  pin_filter->setToolTip(
      QStringLiteral("Keeps the filter bar open even if “Show filter bar” is off."));

  auto* show_sidebar = new QCheckBox(QStringLiteral("Show sidebar (Places / devices)"), &dialog);
  show_sidebar->setChecked(settings->show_sidebar);

  auto* hidden = new QCheckBox(QStringLiteral("Show hidden files"), &dialog);
  hidden->setChecked(settings->show_hidden);

  auto* read_only = new QCheckBox(QStringLiteral("Start in read-only mode"), &dialog);
  read_only->setChecked(settings->read_only);
  read_only->setToolTip(
      QStringLiteral("Blocks delete, rename, paste, mkdir, and similar until toggled off "
                     "(Ctrl+Shift+R)."));

  auto* dismiss_warn = new QCheckBox(QStringLiteral("Don’t show the development warning at startup"),
                                     &dialog);
  dismiss_warn->setChecked(settings->dismiss_dev_warning);

  auto* filter_hint = new QLabel(
      QStringLiteral(
          "QuickFilter chips (types, tags, Untagged…) sit above the filter line when the "
          "bar is visible. Recursive search is separate (F3 / Ctrl+F)."),
      &dialog);
  filter_hint->setWordWrap(true);
  filter_hint->setStyleSheet(QStringLiteral("color: palette(mid);"));

  interface_form->addRow(show_filter);
  interface_form->addRow(pin_filter);
  interface_form->addRow(filter_hint);
  interface_form->addRow(show_sidebar);
  interface_form->addRow(hidden);
  interface_form->addRow(read_only);
  interface_form->addRow(dismiss_warn);
  tabs->addTab(wrap_form(interface_form, &dialog), QStringLiteral("Interface"));

  // ----- Detail columns -----
  auto* columns_page = new QWidget(&dialog);
  auto* columns_layout = new QVBoxLayout(columns_page);
  columns_layout->setContentsMargins(8, 8, 8, 8);
  columns_layout->addWidget(new QLabel(
      QStringLiteral("Columns shown in Detail view (Name is always visible):"), columns_page));

  struct ColOpt {
    const char* key;
    const char* label;
  };
  static constexpr ColOpt kCols[] = {
      {"size", "Size"},
      {"width", "Width"},
      {"height", "Height"},
      {"dimensions", "Dimensions"},
      {"aspectratio", "Aspect ratio"},
      {"framerate", "Framerate"},
      {"duration", "Duration"},
      {"modified", "Modified"},
      {"accessed", "Accessed"},
      {"changed", "Changed"},
      {"birth", "Birth / created"},
      {"type", "Type"},
  };
  QList<QCheckBox*> col_boxes;
  for (const auto& c : kCols) {
    auto* cb = new QCheckBox(QString::fromUtf8(c.label), columns_page);
    cb->setProperty("dirtoo_col_key", QString::fromUtf8(c.key));
    const bool on = settings->detail_columns.contains(QString::fromUtf8(c.key), Qt::CaseInsensitive);
    cb->setChecked(on);
    columns_layout->addWidget(cb);
    col_boxes.append(cb);
  }
  columns_layout->addStretch(1);
  tabs->addTab(columns_page, QStringLiteral("Columns"));

  // ----- Tools -----
  auto* tools_page = new QWidget(&dialog);
  auto* tools_layout = new QVBoxLayout(tools_page);
  tools_layout->setContentsMargins(8, 8, 8, 8);

  auto* tools_intro = new QLabel(
      QStringLiteral(
          "Checksums and tags are stored under your config directory and keyed by file "
          "content (SHA-256). Filter language supports tag:, tagged:, and checksummed: "
          "(see Help → Filter expression help)."),
      tools_page);
  tools_intro->setWordWrap(true);
  tools_layout->addWidget(tools_intro);

  auto* checksums_btn = new QPushButton(QStringLiteral("Checksums…"), tools_page);
  checksums_btn->setToolTip(
      QStringLiteral("Compute or inspect cached digests (CRC32 / MD5 / SHA-1 / SHA-256)"));
  QObject::connect(checksums_btn, &QPushButton::clicked, &dialog, [&dialog] {
    show_checksum_dialog(&dialog, QStringList{});
  });
  tools_layout->addWidget(checksums_btn);

  auto* tags_note = new QLabel(
      QStringLiteral(
          "Tag definitions (rename, color, badge) and “Show files” live under "
          "Tools → Tag Manager. Tag the selection with Ctrl+T or the context menu."),
      tools_page);
  tags_note->setWordWrap(true);
  tags_note->setStyleSheet(QStringLiteral("color: palette(mid);"));
  tools_layout->addWidget(tags_note);
  tools_layout->addStretch(1);
  tabs->addTab(tools_page, QStringLiteral("Tools"));

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  auto* layout = new QVBoxLayout(&dialog);
  layout->addWidget(tabs, 1);
  layout->addWidget(buttons);

  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }

  settings->view_mode = view->currentData().toString();
  settings->zoom_icons = zoom_icons->value();
  settings->zoom_list = zoom_list->value();
  settings->zoom_detail = zoom_detail->value();
  settings->zoom_index = settings->zoom_icons; // legacy
  settings->icon_detail_level = icon_detail->value();
  settings->group_mode = group->currentData().toString();
  settings->size_units = size_units->currentData().toString();
  settings->crop_thumbnails = crop->isChecked();
  settings->icon_spacing = icon_spacing->value();
  settings->icon_cell_padding = icon_pad->value();
  settings->directories_first = dirs_first->isChecked();
  settings->sort_key = default_sort->currentData().toString();
  settings->sort_ascending = sort_ascending->isChecked();
  settings->show_filter = show_filter->isChecked();
  settings->filter_pinned = pin_filter->isChecked();
  settings->show_sidebar = show_sidebar->isChecked();
  settings->show_hidden = hidden->isChecked();
  settings->read_only = read_only->isChecked();
  settings->dismiss_dev_warning = dismiss_warn->isChecked();

  QStringList cols;
  for (QCheckBox* cb : col_boxes) {
    if (cb->isChecked()) {
      cols << cb->property("dirtoo_col_key").toString();
    }
  }
  settings->detail_columns = cols;
  return true;
}

} // namespace dirtoo::app

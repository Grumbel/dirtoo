// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tag_manager_dialog.hpp"

#include "dirtoo/tags/tag_def.hpp"
#include "dirtoo/tags/tag_store.hpp"

#include <QAbstractItemView>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace dirtoo::app {
namespace {

bool edit_tag_properties(QWidget* parent, const QString& name, QString* label, QString* color,
                         QString* badge)
{
  QDialog dlg(parent);
  dlg.setWindowTitle(QStringLiteral("Edit Tag — %1").arg(name));
  dlg.setMinimumWidth(420);
  auto* layout = new QVBoxLayout(&dlg);

  auto* form = new QFormLayout();
  auto* name_lbl = new QLabel(name, &dlg);
  name_lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
  form->addRow(QStringLiteral("Name (key)"), name_lbl);

  auto* label_edit = new QLineEdit(*label, &dlg);
  label_edit->setPlaceholderText(QStringLiteral("Display text (defaults to name)"));
  form->addRow(QStringLiteral("Label"), label_edit);

  auto* color_row = new QWidget(&dlg);
  auto* color_layout = new QHBoxLayout(color_row);
  color_layout->setContentsMargins(0, 0, 0, 0);
  auto* color_edit = new QLineEdit(*color, &dlg);
  color_edit->setPlaceholderText(QStringLiteral("#rrggbb or empty for auto"));
  auto* color_btn = new QPushButton(QStringLiteral("Pick…"), color_row);
  color_layout->addWidget(color_edit, 1);
  color_layout->addWidget(color_btn);
  form->addRow(QStringLiteral("Color"), color_row);

  auto* badge_row = new QWidget(&dlg);
  auto* badge_layout = new QHBoxLayout(badge_row);
  badge_layout->setContentsMargins(0, 0, 0, 0);
  auto* badge_edit = new QLineEdit(*badge, &dlg);
  badge_edit->setPlaceholderText(QStringLiteral("Theme icon, file path, or empty"));
  auto* badge_btn = new QPushButton(QStringLiteral("Browse…"), badge_row);
  badge_layout->addWidget(badge_edit, 1);
  badge_layout->addWidget(badge_btn);
  form->addRow(QStringLiteral("Badge"), badge_row);

  layout->addLayout(form);
  auto* hint = new QLabel(
      QStringLiteral("Name is the stable key used in filters and the database. "
                     "Label is shown on thumbnail chips. Color tints the chip; "
                     "badge is an optional small icon (theme name or image path)."),
      &dlg);
  hint->setWordWrap(true);
  layout->addWidget(hint);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(buttons);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  QObject::connect(color_btn, &QPushButton::clicked, &dlg, [color_edit, &dlg] {
    QColor initial = QColor(color_edit->text());
    if (!initial.isValid()) {
      initial = QColor(100, 149, 237);
    }
    const QColor c = QColorDialog::getColor(initial, &dlg, QStringLiteral("Tag color"));
    if (c.isValid()) {
      color_edit->setText(c.name(QColor::HexRgb));
    }
  });
  QObject::connect(badge_btn, &QPushButton::clicked, &dlg, [badge_edit, &dlg] {
    const QString path = QFileDialog::getOpenFileName(
        &dlg, QStringLiteral("Tag badge image"), badge_edit->text(),
        QStringLiteral("Images (*.png *.svg *.jpg *.jpeg *.webp);;All files (*)"));
    if (!path.isEmpty()) {
      badge_edit->setText(path);
    }
  });

  if (dlg.exec() != QDialog::Accepted) {
    return false;
  }
  *label = label_edit->text().trimmed();
  *color = color_edit->text().trimmed();
  *badge = badge_edit->text().trimmed();
  if (!color->isEmpty()) {
    const QColor c(*color);
    if (!c.isValid()) {
      QMessageBox::warning(parent, QStringLiteral("Tag Manager"),
                           QStringLiteral("Invalid color; use #rrggbb or leave empty."));
      return false;
    }
    *color = c.name(QColor::HexRgb);
  }
  return true;
}

} // namespace

TagManagerDialog::TagManagerDialog(QWidget* parent)
    : QDialog(parent)
{
  setWindowTitle(QStringLiteral("Tag Manager"));
  setAttribute(Qt::WA_DeleteOnClose);
  resize(640, 440);

  auto* layout = new QVBoxLayout(this);
  auto* hint = new QLabel(
      QStringLiteral("Name is the stable key (filters / DB). Label, color, and badge are "
                     "display-only. Renaming updates the key; file associations keep the "
                     "tag id."),
      this);
  hint->setWordWrap(true);
  layout->addWidget(hint);

  tree_ = new QTreeWidget(this);
  tree_->setColumnCount(5);
  tree_->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Label"),
                          QStringLiteral("Color"), QStringLiteral("Badge"),
                          QStringLiteral("Files")});
  tree_->setRootIsDecorated(false);
  tree_->setUniformRowHeights(true);
  tree_->setSelectionMode(QAbstractItemView::SingleSelection);
  tree_->setSortingEnabled(true);
  tree_->sortByColumn(0, Qt::AscendingOrder);
  tree_->header()->setStretchLastSection(false);
  tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  tree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  tree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  tree_->header()->setSectionResizeMode(3, QHeaderView::Stretch);
  tree_->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
  layout->addWidget(tree_, 1);

  status_ = new QLabel(this);
  layout->addWidget(status_);

  auto* row = new QHBoxLayout();
  rename_btn_ = new QPushButton(QStringLiteral("Rename…"), this);
  edit_btn_ = new QPushButton(QStringLiteral("Edit…"), this);
  delete_btn_ = new QPushButton(QStringLiteral("Delete…"), this);
  rename_btn_->setEnabled(false);
  edit_btn_->setEnabled(false);
  delete_btn_->setEnabled(false);
  auto* refresh_btn = new QPushButton(QStringLiteral("Refresh"), this);
  row->addWidget(rename_btn_);
  row->addWidget(edit_btn_);
  row->addWidget(delete_btn_);
  row->addWidget(refresh_btn);
  row->addStretch(1);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  row->addWidget(buttons);
  layout->addLayout(row);

  connect(rename_btn_, &QPushButton::clicked, this, &TagManagerDialog::rename_selected);
  connect(edit_btn_, &QPushButton::clicked, this, &TagManagerDialog::edit_selected);
  connect(delete_btn_, &QPushButton::clicked, this, &TagManagerDialog::delete_selected);
  connect(refresh_btn, &QPushButton::clicked, this, &TagManagerDialog::reload);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(tree_, &QTreeWidget::itemSelectionChanged, this, &TagManagerDialog::on_selection_changed);
  connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem*, int col) {
    if (col == 0) {
      rename_selected();
    } else {
      edit_selected();
    }
  });

  reload();
}

void TagManagerDialog::on_selection_changed()
{
  const bool has = !tree_->selectedItems().isEmpty();
  rename_btn_->setEnabled(has);
  edit_btn_->setEnabled(has);
  delete_btn_->setEnabled(has);
}

void TagManagerDialog::reload()
{
  tree_->clear();
  dirtoo::tags::TagStore store;
  std::string err;
  if (!store.open(dirtoo::tags::TagStore::default_path(), &err)) {
    status_->setText(QStringLiteral("Could not open tags database: %1")
                         .arg(QString::fromStdString(err)));
    rename_btn_->setEnabled(false);
    edit_btn_->setEnabled(false);
    delete_btn_->setEnabled(false);
    return;
  }

  const auto tags = store.list_tags();
  for (const auto& def : tags) {
    auto* item = new QTreeWidgetItem(tree_);
    item->setText(0, QString::fromStdString(def.name));
    const QString label = def.label.empty() ? QString::fromStdString(def.name)
                                            : QString::fromStdString(def.label);
    item->setText(1, label);
    item->setText(2, QString::fromStdString(def.color));
    item->setText(3, QString::fromStdString(def.badge));
    const auto n = store.count_files_for_tag(def.name);
    item->setText(4, QString::number(static_cast<qulonglong>(n)));
    item->setData(0, Qt::UserRole, QString::fromStdString(def.name));
    if (!def.color.empty()) {
      const QColor c(QString::fromStdString(def.color));
      if (c.isValid()) {
        item->setBackground(2, c);
        const int lum = (c.red() * 299 + c.green() * 587 + c.blue() * 114) / 1000;
        item->setForeground(2, lum > 140 ? QColor(20, 20, 20) : QColor(250, 250, 250));
      }
    }
  }
  status_->setText(tags.empty() ? QStringLiteral("No tags defined yet. Use Tools → Tag… on files.")
                                : QStringLiteral("%1 tag(s)").arg(static_cast<int>(tags.size())));
  on_selection_changed();
}

void TagManagerDialog::rename_selected()
{
  const auto selected = tree_->selectedItems();
  if (selected.isEmpty()) {
    return;
  }
  const QString old_name = selected.front()->data(0, Qt::UserRole).toString();
  if (old_name.isEmpty()) {
    return;
  }

  bool ok = false;
  const QString typed = QInputDialog::getText(
      this, QStringLiteral("Rename Tag"),
      QStringLiteral("New name (key) for “%1” — letters, digits, _ / -:").arg(old_name),
      QLineEdit::Normal, old_name, &ok);
  if (!ok) {
    return;
  }
  const std::string normalized = dirtoo::tags::normalize_tag_name(typed.toStdString());
  if (normalized.empty()) {
    QMessageBox::warning(this, QStringLiteral("Tag Manager"),
                         QStringLiteral("Invalid tag name."));
    return;
  }
  const QString new_name = QString::fromStdString(normalized);

  dirtoo::tags::TagStore store;
  std::string err;
  if (!store.open(dirtoo::tags::TagStore::default_path(), &err)) {
    QMessageBox::warning(this, QStringLiteral("Tag Manager"),
                         QStringLiteral("Could not open tags database:\n%1")
                             .arg(QString::fromStdString(err)));
    return;
  }
  if (!store.rename_tag(old_name.toStdString(), new_name.toStdString(), &err)) {
    QMessageBox::warning(this, QStringLiteral("Tag Manager"),
                         QStringLiteral("Rename failed:\n%1").arg(QString::fromStdString(err)));
    return;
  }

  emit tags_changed();
  reload();
}

void TagManagerDialog::edit_selected()
{
  const auto selected = tree_->selectedItems();
  if (selected.isEmpty()) {
    return;
  }
  const QString name = selected.front()->data(0, Qt::UserRole).toString();
  if (name.isEmpty()) {
    return;
  }

  dirtoo::tags::TagStore store;
  std::string err;
  if (!store.open(dirtoo::tags::TagStore::default_path(), &err)) {
    QMessageBox::warning(this, QStringLiteral("Tag Manager"),
                         QStringLiteral("Could not open tags database:\n%1")
                             .arg(QString::fromStdString(err)));
    return;
  }
  auto def = store.get_tag(name.toStdString());
  if (!def) {
    QMessageBox::warning(this, QStringLiteral("Tag Manager"), QStringLiteral("Unknown tag."));
    return;
  }

  QString label = QString::fromStdString(def->label);
  QString color = QString::fromStdString(def->color);
  QString badge = QString::fromStdString(def->badge);
  if (!edit_tag_properties(this, name, &label, &color, &badge)) {
    return;
  }

  // Empty label → store name so display falls back cleanly.
  const std::string label_s =
      label.isEmpty() ? def->name : label.toStdString();
  if (!store.set_tag_meta(name.toStdString(), label_s, color.toStdString(), badge.toStdString(),
                          &err)) {
    QMessageBox::warning(this, QStringLiteral("Tag Manager"),
                         QStringLiteral("Could not save tag properties:\n%1")
                             .arg(QString::fromStdString(err)));
    return;
  }

  emit tags_changed();
  reload();
}

void TagManagerDialog::delete_selected()
{
  const auto selected = tree_->selectedItems();
  if (selected.isEmpty()) {
    return;
  }
  const QString name = selected.front()->data(0, Qt::UserRole).toString();
  if (name.isEmpty()) {
    return;
  }
  const QString files = selected.front()->text(4);
  const auto reply = QMessageBox::question(
      this, QStringLiteral("Delete Tag"),
      QStringLiteral("Delete tag “%1” (%2 file association(s))?\n\n"
                     "Files themselves are not deleted; only the tag definition "
                     "and its associations are removed.")
          .arg(name, files),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (reply != QMessageBox::Yes) {
    return;
  }

  dirtoo::tags::TagStore store;
  std::string err;
  if (!store.open(dirtoo::tags::TagStore::default_path(), &err)) {
    QMessageBox::warning(this, QStringLiteral("Tag Manager"),
                         QStringLiteral("Could not open tags database:\n%1")
                             .arg(QString::fromStdString(err)));
    return;
  }
  if (!store.delete_tag(name.toStdString(), &err)) {
    QMessageBox::warning(this, QStringLiteral("Tag Manager"),
                         QStringLiteral("Delete failed:\n%1").arg(QString::fromStdString(err)));
    return;
  }

  emit tags_changed();
  reload();
}

void show_tag_manager_dialog(QWidget* parent)
{
  auto* dlg = new TagManagerDialog(parent);
  dlg->show();
  dlg->raise();
  dlg->activateWindow();
}

} // namespace dirtoo::app

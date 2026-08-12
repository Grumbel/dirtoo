// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tag_manager_dialog.hpp"

#include "dirtoo/tags/tag_def.hpp"
#include <QInputDialog>
#include <QLineEdit>
#include "dirtoo/tags/tag_store.hpp"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace dirtoo::app {

TagManagerDialog::TagManagerDialog(QWidget* parent)
    : QDialog(parent)
{
  setWindowTitle(QStringLiteral("Tag Manager"));
  setAttribute(Qt::WA_DeleteOnClose);
  resize(520, 420);

  auto* layout = new QVBoxLayout(this);
  auto* hint = new QLabel(
      QStringLiteral("Tags are stored by content identity (SHA-256). Renaming a tag "
                     "updates the definition only; files stay associated."),
      this);
  hint->setWordWrap(true);
  layout->addWidget(hint);

  tree_ = new QTreeWidget(this);
  tree_->setColumnCount(3);
  tree_->setHeaderLabels(
      {QStringLiteral("Name"), QStringLiteral("Label"), QStringLiteral("Files")});
  tree_->setRootIsDecorated(false);
  tree_->setUniformRowHeights(true);
  tree_->setSelectionMode(QAbstractItemView::SingleSelection);
  tree_->setSortingEnabled(true);
  tree_->sortByColumn(0, Qt::AscendingOrder);
  tree_->header()->setStretchLastSection(false);
  tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  tree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  tree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  layout->addWidget(tree_, 1);

  status_ = new QLabel(this);
  layout->addWidget(status_);

  auto* row = new QHBoxLayout();
  rename_btn_ = new QPushButton(QStringLiteral("Rename…"), this);
  rename_btn_->setEnabled(false);
  auto* refresh_btn = new QPushButton(QStringLiteral("Refresh"), this);
  row->addWidget(rename_btn_);
  row->addWidget(refresh_btn);
  row->addStretch(1);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  row->addWidget(buttons);
  layout->addLayout(row);

  connect(rename_btn_, &QPushButton::clicked, this, &TagManagerDialog::rename_selected);
  connect(refresh_btn, &QPushButton::clicked, this, &TagManagerDialog::reload);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(tree_, &QTreeWidget::itemSelectionChanged, this, &TagManagerDialog::on_selection_changed);
  connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem*, int) {
    rename_selected();
  });

  reload();
}

void TagManagerDialog::on_selection_changed()
{
  rename_btn_->setEnabled(!tree_->selectedItems().isEmpty());
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
    return;
  }

  const auto tags = store.list_tags();
  for (const auto& def : tags) {
    auto* item = new QTreeWidgetItem(tree_);
    item->setText(0, QString::fromStdString(def.name));
    const QString label = def.label.empty() ? QString::fromStdString(def.name)
                                            : QString::fromStdString(def.label);
    item->setText(1, label);
    const auto files = store.files_for_tag(def.name);
    item->setText(2, QString::number(static_cast<qulonglong>(files.size())));
    item->setData(0, Qt::UserRole, QString::fromStdString(def.name));
    if (!def.color.empty()) {
      item->setToolTip(0, QStringLiteral("Color: %1").arg(QString::fromStdString(def.color)));
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
      QStringLiteral("New name for tag “%1” (letters, digits, _ / -):").arg(old_name),
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

void show_tag_manager_dialog(QWidget* parent)
{
  auto* dlg = new TagManagerDialog(parent);
  dlg->show();
  dlg->raise();
  dlg->activateWindow();
}

} // namespace dirtoo::app

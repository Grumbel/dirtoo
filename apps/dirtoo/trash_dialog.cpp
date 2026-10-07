// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "trash_dialog.hpp"

#include "async_io.hpp"
#include "operations_history.hpp"
#include "theme_icons.hpp"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace dirtoo::app {
namespace {

enum Column { ColName, ColLocation, ColDeleted, ColCount };

struct OpOutcome {
  dirops::TrashEntry entry;
  bool ok = false;
  QString error;
};

} // namespace

TrashDialog::TrashDialog(QWidget* parent)
    : QDialog(parent)
{
  setWindowTitle(QStringLiteral("Trash"));
  resize(760, 440);

  auto* layout = new QVBoxLayout(this);
  table_ = new QTableWidget(0, ColCount, this);
  table_->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Original location"),
                                     QStringLiteral("Deleted")});
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->verticalHeader()->setVisible(false);
  table_->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(ColLocation, QHeaderView::Stretch);
  table_->horizontalHeader()->setSectionResizeMode(ColDeleted, QHeaderView::ResizeToContents);
  layout->addWidget(table_, 1);

  status_ = new QLabel(this);
  layout->addWidget(status_);

  auto* buttons = new QDialogButtonBox(this);
  empty_btn_ = buttons->addButton(QStringLiteral("Empty Trash…"), QDialogButtonBox::ActionRole);
  delete_btn_ = buttons->addButton(QStringLiteral("Delete Permanently…"), QDialogButtonBox::ActionRole);
  restore_btn_ = buttons->addButton(QStringLiteral("Restore"), QDialogButtonBox::AcceptRole);
  buttons->addButton(QDialogButtonBox::Close);
  layout->addWidget(buttons);

  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
  connect(restore_btn_, &QPushButton::clicked, this, &TrashDialog::restore_selected);
  connect(delete_btn_, &QPushButton::clicked, this, &TrashDialog::delete_selected);
  connect(empty_btn_, &QPushButton::clicked, this, &TrashDialog::empty_trash);
  connect(table_, &QTableWidget::itemSelectionChanged, this, &TrashDialog::update_buttons);
  connect(table_, &QTableWidget::itemDoubleClicked, this, [this] { restore_selected(); });

  update_buttons();
  reload();
}

void TrashDialog::update_buttons()
{
  const bool any = !table_->selectionModel()->selectedRows().isEmpty();
  restore_btn_->setEnabled(any && !busy_);
  delete_btn_->setEnabled(any && !busy_);
  empty_btn_->setEnabled(!entries_.empty() && !busy_);
}

void TrashDialog::reload()
{
  busy_ = true;
  status_->setText(QStringLiteral("Reading the trash…"));
  update_buttons();
  run_io(
      this, [] { return dirops::list_trash(); },
      [this](std::vector<dirops::TrashEntry> entries) {
        busy_ = false;
        // Newest first.
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
          return a.deletion_date > b.deletion_date;
        });
        entries_ = std::move(entries);
        table_->setRowCount(static_cast<int>(entries_.size()));
        for (int row = 0; row < static_cast<int>(entries_.size()); ++row) {
          const auto& e = entries_[static_cast<std::size_t>(row)];
          auto* name = new QTableWidgetItem(QString::fromStdString(e.original_path.filename().string()));
          name->setToolTip(QString::fromStdString(e.original_path.string()));
          table_->setItem(row, ColName, name);
          table_->setItem(row, ColLocation,
                          new QTableWidgetItem(
                              QString::fromStdString(e.original_path.parent_path().string())));
          table_->setItem(row, ColDeleted,
                          new QTableWidgetItem(QString::fromStdString(e.deletion_date).replace(
                              QLatin1Char('T'), QLatin1Char(' '))));
        }
        status_->setText(entries_.empty() ? QStringLiteral("The trash is empty")
                                          : QStringLiteral("%1 item(s)").arg(entries_.size()));
        update_buttons();
      });
}

std::vector<dirops::TrashEntry> TrashDialog::selected_entries() const
{
  std::vector<dirops::TrashEntry> out;
  for (const QModelIndex& idx : table_->selectionModel()->selectedRows()) {
    if (idx.row() >= 0 && static_cast<std::size_t>(idx.row()) < entries_.size()) {
      out.push_back(entries_[static_cast<std::size_t>(idx.row())]);
    }
  }
  return out;
}

void TrashDialog::run_on_entries(const std::vector<dirops::TrashEntry>& entries, bool restore)
{
  if (entries.empty() || busy_) {
    return;
  }
  busy_ = true;
  status_->setText(restore ? QStringLiteral("Restoring…") : QStringLiteral("Deleting…"));
  update_buttons();
  run_io(
      this,
      [entries, restore] {
        std::vector<OpOutcome> out;
        for (const auto& e : entries) {
          OpOutcome o;
          o.entry = e;
          auto r = restore ? dirops::restore_trash_entry(e) : dirops::delete_trash_entry(e);
          o.ok = r.has_value();
          if (!r) {
            o.error = QString::fromStdString(r.error().to_string());
          }
          out.push_back(std::move(o));
        }
        return out;
      },
      [this, restore](std::vector<OpOutcome> outcomes) {
        busy_ = false;
        QStringList problems;
        for (const auto& o : outcomes) {
          // The history is GUI-thread state: record here.
          operations_history().record_simple(
              restore ? OperationKind::Restore : OperationKind::Delete, {o.entry.stored_path()},
              restore ? o.entry.original_path : std::filesystem::path{}, o.ok, o.error);
          if (!o.ok) {
            problems << QStringLiteral("%1: %2")
                            .arg(QString::fromStdString(o.entry.original_path.filename().string()),
                                 o.error);
          }
        }
        if (!problems.isEmpty()) {
          QMessageBox::warning(this, QStringLiteral("Trash"), problems.join(QLatin1Char('\n')));
        }
        emit filesystem_changed();
        reload();
      });
}

void TrashDialog::restore_selected()
{
  run_on_entries(selected_entries(), /*restore=*/true);
}

void TrashDialog::delete_selected()
{
  const auto entries = selected_entries();
  if (entries.empty()) {
    return;
  }
  const auto answer = QMessageBox::question(
      this, QStringLiteral("Delete Permanently"),
      QStringLiteral("Permanently delete %1 item(s) from the trash?\nThis cannot be undone.")
          .arg(entries.size()));
  if (answer == QMessageBox::Yes) {
    run_on_entries(entries, /*restore=*/false);
  }
}

void TrashDialog::empty_trash()
{
  if (entries_.empty() || busy_) {
    return;
  }
  const auto answer = QMessageBox::question(
      this, QStringLiteral("Empty Trash"),
      QStringLiteral("Permanently delete all %1 item(s) in the trash?\nThis cannot be undone.")
          .arg(entries_.size()));
  if (answer != QMessageBox::Yes) {
    return;
  }
  busy_ = true;
  status_->setText(QStringLiteral("Emptying the trash…"));
  update_buttons();
  const auto entries = entries_;
  run_io(
      this, [] { return dirops::empty_trash(); },
      [this, entries](dirops::OpResult result) {
        busy_ = false;
        const bool ok = result.has_value();
        const QString err = ok ? QString() : QString::fromStdString(result.error().to_string());
        std::vector<std::filesystem::path> sources;
        for (const auto& e : entries) {
          sources.push_back(e.stored_path());
        }
        operations_history().record_simple(OperationKind::Delete, sources, {}, ok, err);
        if (!ok) {
          QMessageBox::warning(this, QStringLiteral("Empty Trash"), err);
        }
        emit filesystem_changed();
        reload();
      });
}

} // namespace dirtoo::app

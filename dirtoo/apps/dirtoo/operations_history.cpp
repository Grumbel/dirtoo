// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "operations_history.hpp"

#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <utility>

namespace dirtoo::app {
namespace {

QString paths_label(const QStringList& paths)
{
  if (paths.isEmpty()) {
    return QStringLiteral("—");
  }
  if (paths.size() == 1) {
    return paths.front();
  }
  return QStringLiteral("%1 (+%2 more)").arg(paths.front()).arg(paths.size() - 1);
}

QString parent_dir_of(const QString& path)
{
  const QFileInfo fi(path);
  if (fi.exists()) {
    return fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
  }
  // Path may no longer exist (delete); still use parent of the string path.
  const auto p = std::filesystem::path{path.toStdString()}.parent_path();
  return QString::fromStdString(p.string());
}

} // namespace

QString operation_kind_label(OperationKind kind)
{
  switch (kind) {
  case OperationKind::Copy:
    return QStringLiteral("Copy");
  case OperationKind::Move:
    return QStringLiteral("Move");
  case OperationKind::Rename:
    return QStringLiteral("Rename");
  case OperationKind::Delete:
    return QStringLiteral("Delete");
  case OperationKind::Mkdir:
    return QStringLiteral("New folder");
  case OperationKind::Mkfile:
    return QStringLiteral("New file");
  case OperationKind::Symlink:
    return QStringLiteral("Symlink");
  case OperationKind::Swap:
    return QStringLiteral("Swap names");
  case OperationKind::Permissions:
    return QStringLiteral("Permissions");
  case OperationKind::Other:
  default:
    return QStringLiteral("Other");
  }
}

QString operation_kind_to_string(OperationKind kind)
{
  switch (kind) {
  case OperationKind::Copy:
    return QStringLiteral("copy");
  case OperationKind::Move:
    return QStringLiteral("move");
  case OperationKind::Rename:
    return QStringLiteral("rename");
  case OperationKind::Delete:
    return QStringLiteral("delete");
  case OperationKind::Mkdir:
    return QStringLiteral("mkdir");
  case OperationKind::Mkfile:
    return QStringLiteral("mkfile");
  case OperationKind::Symlink:
    return QStringLiteral("symlink");
  case OperationKind::Swap:
    return QStringLiteral("swap");
  case OperationKind::Permissions:
    return QStringLiteral("permissions");
  case OperationKind::Other:
  default:
    return QStringLiteral("other");
  }
}

OperationKind operation_kind_from_string(const QString& s)
{
  const QString k = s.toLower();
  if (k == QLatin1String("copy")) {
    return OperationKind::Copy;
  }
  if (k == QLatin1String("move")) {
    return OperationKind::Move;
  }
  if (k == QLatin1String("rename")) {
    return OperationKind::Rename;
  }
  if (k == QLatin1String("delete")) {
    return OperationKind::Delete;
  }
  if (k == QLatin1String("mkdir")) {
    return OperationKind::Mkdir;
  }
  if (k == QLatin1String("mkfile")) {
    return OperationKind::Mkfile;
  }
  if (k == QLatin1String("symlink")) {
    return OperationKind::Symlink;
  }
  if (k == QLatin1String("swap")) {
    return OperationKind::Swap;
  }
  if (k == QLatin1String("permissions")) {
    return OperationKind::Permissions;
  }
  return OperationKind::Other;
}

OperationsHistory::OperationsHistory(std::filesystem::path file)
    : path_(std::move(file))
{
  load();
}

std::filesystem::path OperationsHistory::default_path()
{
  const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  std::filesystem::path dir{base.toStdString()};
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir / "operations-history.txt";
}

OperationsHistory& operations_history()
{
  static OperationsHistory instance{OperationsHistory::default_path()};
  return instance;
}

void OperationsHistory::record(OperationHistoryEntry entry)
{
  if (!entry.when.isValid()) {
    entry.when = QDateTime::currentDateTime();
  }
  entries_.push_back(std::move(entry));
  while (static_cast<int>(entries_.size()) > kMaxEntries) {
    entries_.erase(entries_.begin());
  }
  save();
}

void OperationsHistory::record_simple(OperationKind kind,
                                      const std::vector<std::filesystem::path>& sources,
                                      const std::filesystem::path& destination, bool ok,
                                      const QString& detail)
{
  OperationHistoryEntry e;
  e.when = QDateTime::currentDateTime();
  e.kind = kind;
  for (const auto& p : sources) {
    e.sources << QString::fromStdString(p.string());
  }
  if (!destination.empty()) {
    e.destination = QString::fromStdString(destination.string());
  }
  e.outcome = ok ? QStringLiteral("success") : QStringLiteral("failed");
  e.detail = detail;
  record(std::move(e));
}

std::vector<OperationHistoryEntry> OperationsHistory::entries() const
{
  return entries_;
}

void OperationsHistory::clear()
{
  entries_.clear();
  save();
}

void OperationsHistory::load()
{
  entries_.clear();
  QFile f(QString::fromStdString(path_.string()));
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return;
  }
  QTextStream in(&f);
  while (!in.atEnd()) {
    const QString line = in.readLine();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
      continue;
    }
    // when_iso | kind | outcome | dest | detail | path1 | path2 | …
    const QStringList parts = line.split(QLatin1Char('\t'));
    if (parts.size() < 5) {
      continue;
    }
    OperationHistoryEntry e;
    e.when = QDateTime::fromString(parts[0], Qt::ISODate);
    e.kind = operation_kind_from_string(parts[1]);
    e.outcome = parts[2];
    e.destination = parts[3];
    e.detail = parts[4];
    for (int i = 5; i < parts.size(); ++i) {
      if (!parts[i].isEmpty()) {
        e.sources << parts[i];
      }
    }
    entries_.push_back(std::move(e));
  }
}

void OperationsHistory::save() const
{
  QFile f(QString::fromStdString(path_.string()));
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
    return;
  }
  QTextStream out(&f);
  out << QStringLiteral("# dirtoo operations history (tab-separated)\n");
  for (const auto& e : entries_) {
    out << e.when.toString(Qt::ISODate) << QLatin1Char('\t')
        << operation_kind_to_string(e.kind) << QLatin1Char('\t') << e.outcome
        << QLatin1Char('\t') << e.destination << QLatin1Char('\t') << e.detail;
    for (const QString& s : e.sources) {
      out << QLatin1Char('\t') << s;
    }
    out << QLatin1Char('\n');
  }
}

void show_operations_history_dialog(
    QWidget* parent, std::function<void(const QString& directory)> on_go_to_folder)
{
  auto* dialog = new QDialog(parent);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(QStringLiteral("Operations History"));
  dialog->resize(820, 440);

  auto* layout = new QVBoxLayout(dialog);
  auto* filter = new QLineEdit(dialog);
  filter->setPlaceholderText(QStringLiteral("Filter by operation, path, or outcome…"));
  filter->setClearButtonEnabled(true);
  layout->addWidget(filter);

  auto* tree = new QTreeWidget(dialog);
  tree->setColumnCount(5);
  tree->setHeaderLabels({QStringLiteral("When"), QStringLiteral("Operation"),
                         QStringLiteral("Outcome"), QStringLiteral("Destination"),
                         QStringLiteral("Sources")});
  tree->setRootIsDecorated(false);
  tree->setAlternatingRowColors(true);
  tree->setSelectionMode(QAbstractItemView::SingleSelection);
  tree->setUniformRowHeights(true);
  tree->header()->setStretchLastSection(true);
  tree->header()->resizeSection(0, 150);
  tree->header()->resizeSection(1, 100);
  tree->header()->resizeSection(2, 80);
  tree->header()->resizeSection(3, 220);
  layout->addWidget(tree, 1);

  const auto refill = [tree](const QString& needle) {
    tree->clear();
    const QString n = needle.trimmed().toLower();
    const auto all = operations_history().entries();
    for (auto it = all.rbegin(); it != all.rend(); ++it) {
      const OperationHistoryEntry& e = *it;
      const QString sources = paths_label(e.sources);
      if (!n.isEmpty()) {
        const QString blob =
            (operation_kind_label(e.kind) + e.outcome + e.destination + sources + e.detail)
                .toLower();
        if (!blob.contains(n)) {
          continue;
        }
      }
      auto* item = new QTreeWidgetItem(tree);
      item->setText(0, e.when.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
      item->setText(1, operation_kind_label(e.kind));
      item->setText(2, e.outcome);
      item->setText(3, e.destination.isEmpty() ? QStringLiteral("—") : e.destination);
      item->setText(4, sources);
      item->setToolTip(4, e.sources.join(QLatin1Char('\n')));
      if (!e.detail.isEmpty()) {
        item->setToolTip(2, e.detail);
      }
      // Store a navigable path in UserRole.
      QString go = e.destination;
      if (go.isEmpty() && !e.sources.isEmpty()) {
        go = e.sources.front();
      }
      item->setData(0, Qt::UserRole, go);
    }
  };
  refill({});
  QObject::connect(filter, &QLineEdit::textChanged, dialog, [refill](const QString& text) {
    refill(text);
  });

  auto* buttons = new QDialogButtonBox(dialog);
  auto* go_btn = buttons->addButton(QStringLiteral("Go to Folder"), QDialogButtonBox::ActionRole);
  auto* clear_btn = buttons->addButton(QStringLiteral("Clear History"), QDialogButtonBox::ActionRole);
  buttons->addButton(QDialogButtonBox::Close);
  layout->addWidget(buttons);

  QObject::connect(go_btn, &QPushButton::clicked, dialog, [tree, dialog, on_go_to_folder] {
    const auto items = tree->selectedItems();
    if (items.isEmpty()) {
      return;
    }
    const QString path = items.front()->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) {
      return;
    }
    const QString dir = parent_dir_of(path);
    if (on_go_to_folder) {
      on_go_to_folder(dir);
      dialog->accept();
    }
  });
  QObject::connect(clear_btn, &QPushButton::clicked, dialog, [dialog, refill] {
    if (QMessageBox::question(dialog, QStringLiteral("Clear Operations History"),
                              QStringLiteral("Remove all logged operations?"))
        == QMessageBox::Yes) {
      operations_history().clear();
      refill({});
    }
  });
  QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

  dialog->show();
}

} // namespace dirtoo::app

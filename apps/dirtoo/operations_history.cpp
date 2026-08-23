// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "operations_history.hpp"

#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <sqlite3.h>

#include <cstdlib>
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
  const auto p = std::filesystem::path{path.toStdString()}.parent_path();
  return QString::fromStdString(p.string());
}

QString json_string_array(const QStringList& list)
{
  QJsonArray arr;
  for (const QString& s : list) {
    arr.append(s);
  }
  return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QStringList parse_json_string_array(const QString& json)
{
  QStringList out;
  if (json.isEmpty()) {
    return out;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
  if (!doc.isArray()) {
    return out;
  }
  for (const QJsonValue& v : doc.array()) {
    out << v.toString();
  }
  return out;
}

QString json_items(const std::vector<OperationItem>& items)
{
  QJsonArray arr;
  for (const auto& it : items) {
    QJsonObject o;
    o.insert(QStringLiteral("source"), it.source);
    o.insert(QStringLiteral("destination"), it.destination);
    o.insert(QStringLiteral("skipped"), it.skipped);
    arr.append(o);
  }
  return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

std::vector<OperationItem> parse_json_items(const QString& json)
{
  std::vector<OperationItem> out;
  if (json.isEmpty()) {
    return out;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
  if (!doc.isArray()) {
    return out;
  }
  for (const QJsonValue& v : doc.array()) {
    const QJsonObject o = v.toObject();
    OperationItem it;
    it.source = o.value(QStringLiteral("source")).toString();
    it.destination = o.value(QStringLiteral("destination")).toString();
    it.skipped = o.value(QStringLiteral("skipped")).toBool();
    out.push_back(std::move(it));
  }
  return out;
}

std::filesystem::path xdg_state_home()
{
  if (const char* env = std::getenv("XDG_STATE_HOME"); env != nullptr && env[0] != '\0') {
    return std::filesystem::path{env};
  }
  const char* home = std::getenv("HOME");
  if (home != nullptr && home[0] != '\0') {
    return std::filesystem::path{home} / ".local" / "state";
  }
  return std::filesystem::path{"."} / ".local" / "state";
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

OperationsHistory::OperationsHistory(std::filesystem::path db_path)
    : path_(std::move(db_path))
{
  open_db();
}

OperationsHistory::~OperationsHistory()
{
  close_db();
}

std::filesystem::path OperationsHistory::default_path()
{
  const auto dir = xdg_state_home() / "dirtoo";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir / "operations-history.sqlite";
}

OperationsHistory& operations_history()
{
  static OperationsHistory instance{OperationsHistory::default_path()};
  return instance;
}

void OperationsHistory::close_db()
{
  if (db_ != nullptr) {
    sqlite3_close(static_cast<sqlite3*>(db_));
    db_ = nullptr;
  }
}

void OperationsHistory::open_db()
{
  close_db();
  sqlite3* raw = nullptr;
  if (sqlite3_open(path_.string().c_str(), &raw) != SQLITE_OK) {
    if (raw != nullptr) {
      sqlite3_close(raw);
    }
    db_ = nullptr;
    return;
  }
  db_ = raw;
  char* err = nullptr;
  const char* schema =
      "CREATE TABLE IF NOT EXISTS operations ("
      "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  when_iso TEXT NOT NULL,"
      "  kind TEXT NOT NULL,"
      "  outcome TEXT NOT NULL,"
      "  destination TEXT,"
      "  detail TEXT,"
      "  completed INTEGER DEFAULT 0,"
      "  skipped INTEGER DEFAULT 0,"
      "  sources_json TEXT NOT NULL DEFAULT '[]',"
      "  destinations_json TEXT NOT NULL DEFAULT '[]',"
      "  items_json TEXT NOT NULL DEFAULT '[]'"
      ");"
      "CREATE INDEX IF NOT EXISTS idx_operations_when ON operations(when_iso);";
  if (sqlite3_exec(raw, schema, nullptr, nullptr, &err) != SQLITE_OK) {
    if (err != nullptr) {
      sqlite3_free(err);
    }
  }
}

void OperationsHistory::record(OperationHistoryEntry entry)
{
  if (db_ == nullptr) {
    open_db();
  }
  if (db_ == nullptr) {
    return;
  }
  if (!entry.when.isValid()) {
    entry.when = QDateTime::currentDateTime();
  }
  if (entry.sources.isEmpty() && !entry.items.empty()) {
    for (const auto& it : entry.items) {
      if (!it.source.isEmpty()) {
        entry.sources << it.source;
      }
      if (!it.destination.isEmpty()) {
        entry.destinations << it.destination;
      }
    }
  }
  if (entry.destination.isEmpty() && !entry.destinations.isEmpty()) {
    entry.destination = entry.destinations.front();
  }

  sqlite3* raw = static_cast<sqlite3*>(db_);
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "INSERT INTO operations(when_iso, kind, outcome, destination, detail, completed, skipped, "
      "sources_json, destinations_json, items_json) VALUES (?,?,?,?,?,?,?,?,?,?);";
  if (sqlite3_prepare_v2(raw, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  const QByteArray when = entry.when.toString(Qt::ISODate).toUtf8();
  const QByteArray kind = operation_kind_to_string(entry.kind).toUtf8();
  const QByteArray outcome = entry.outcome.toUtf8();
  const QByteArray dest = entry.destination.toUtf8();
  const QByteArray detail = entry.detail.toUtf8();
  const QByteArray sources = json_string_array(entry.sources).toUtf8();
  const QByteArray dests = json_string_array(entry.destinations).toUtf8();
  const QByteArray items = json_items(entry.items).toUtf8();

  sqlite3_bind_text(stmt, 1, when.constData(), when.size(), SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, kind.constData(), kind.size(), SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, outcome.constData(), outcome.size(), SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, dest.constData(), dest.size(), SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, detail.constData(), detail.size(), SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 6, entry.completed);
  sqlite3_bind_int(stmt, 7, entry.skipped);
  sqlite3_bind_text(stmt, 8, sources.constData(), sources.size(), SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 9, dests.constData(), dests.size(), SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 10, items.constData(), items.size(), SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  sqlite3_exec(raw,
               "DELETE FROM operations WHERE id NOT IN "
               "(SELECT id FROM operations ORDER BY id DESC LIMIT 1000);",
               nullptr, nullptr, nullptr);
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
    e.destinations << e.destination;
  }
  if (e.sources.isEmpty() && !e.destination.isEmpty()) {
    OperationItem it;
    it.destination = e.destination;
    e.items.push_back(std::move(it));
  } else {
    for (const QString& src : e.sources) {
      OperationItem it;
      it.source = src;
      it.destination = e.destination;
      e.items.push_back(std::move(it));
    }
  }
  e.outcome = ok ? QStringLiteral("success") : QStringLiteral("failed");
  e.detail = detail;
  e.completed = ok ? static_cast<int>(e.sources.isEmpty() ? 1 : e.sources.size()) : 0;
  record(std::move(e));
}

std::vector<OperationHistoryEntry> OperationsHistory::entries() const
{
  std::vector<OperationHistoryEntry> out;
  if (db_ == nullptr) {
    return out;
  }
  sqlite3* raw = static_cast<sqlite3*>(db_);
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT id, when_iso, kind, outcome, destination, detail, completed, skipped, "
      "sources_json, destinations_json, items_json FROM operations ORDER BY id ASC;";
  if (sqlite3_prepare_v2(raw, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return out;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    OperationHistoryEntry e;
    e.id = sqlite3_column_int64(stmt, 0);
    const auto col = [](sqlite3_stmt* s, int i) {
      const unsigned char* p = sqlite3_column_text(s, i);
      return p ? QString::fromUtf8(reinterpret_cast<const char*>(p)) : QString();
    };
    e.when = QDateTime::fromString(col(stmt, 1), Qt::ISODate);
    e.kind = operation_kind_from_string(col(stmt, 2));
    e.outcome = col(stmt, 3);
    e.destination = col(stmt, 4);
    e.detail = col(stmt, 5);
    e.completed = sqlite3_column_int(stmt, 6);
    e.skipped = sqlite3_column_int(stmt, 7);
    e.sources = parse_json_string_array(col(stmt, 8));
    e.destinations = parse_json_string_array(col(stmt, 9));
    e.items = parse_json_items(col(stmt, 10));
    out.push_back(std::move(e));
  }
  sqlite3_finalize(stmt);
  return out;
}

void OperationsHistory::clear()
{
  if (db_ == nullptr) {
    return;
  }
  sqlite3_exec(static_cast<sqlite3*>(db_), "DELETE FROM operations;", nullptr, nullptr, nullptr);
}

void show_operations_history_dialog(
    QWidget* parent, std::function<void(const QString& directory)> on_go_to_folder)
{
  auto* dialog = new QDialog(parent);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(QStringLiteral("Operations History"));
  dialog->resize(900, 480);

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
  tree->setRootIsDecorated(true);
  tree->setAlternatingRowColors(true);
  tree->setSelectionMode(QAbstractItemView::SingleSelection);
  tree->header()->setStretchLastSection(true);
  tree->header()->resizeSection(0, 150);
  tree->header()->resizeSection(1, 100);
  tree->header()->resizeSection(2, 90);
  tree->header()->resizeSection(3, 240);
  layout->addWidget(tree, 1);

  const auto refill = [tree](const QString& needle) {
    tree->clear();
    const QString n = needle.trimmed().toLower();
    const auto all = operations_history().entries();
    for (auto it = all.rbegin(); it != all.rend(); ++it) {
      const OperationHistoryEntry& e = *it;
      const QString sources = paths_label(e.sources);
      if (!n.isEmpty()) {
        QString blob =
            (operation_kind_label(e.kind) + e.outcome + e.destination + sources + e.detail)
                .toLower();
        for (const auto& sub : e.items) {
          blob += sub.source.toLower() + sub.destination.toLower();
        }
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
      QString go = e.destination;
      if (go.isEmpty() && !e.sources.isEmpty()) {
        go = e.sources.front();
      }
      item->setData(0, Qt::UserRole, go);

      for (const auto& sub : e.items) {
        if (e.items.size() == 1 && sub.destination == e.destination
            && sub.source == (e.sources.isEmpty() ? QString() : e.sources.front())) {
          continue;
        }
        auto* child = new QTreeWidgetItem(item);
        child->setText(0, sub.skipped ? QStringLiteral("skipped") : QString());
        child->setText(3, sub.destination.isEmpty() ? QStringLiteral("—") : sub.destination);
        child->setText(4, sub.source);
        child->setData(0, Qt::UserRole,
                       sub.destination.isEmpty() ? sub.source : sub.destination);
      }
    }
  };
  refill({});
  QObject::connect(filter, &QLineEdit::textChanged, dialog, [refill](const QString& text) {
    refill(text);
  });

  auto* buttons = new QDialogButtonBox(dialog);
  auto* go_btn = buttons->addButton(QStringLiteral("Go to Folder"), QDialogButtonBox::ActionRole);
  auto* clear_btn =
      buttons->addButton(QStringLiteral("Clear History"), QDialogButtonBox::ActionRole);
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

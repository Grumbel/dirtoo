// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "open_history.hpp"

#include "open_with.hpp"

#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <fstream>
#include <optional>
#include <sstream>

namespace dirtoo::app {
namespace {

QString escape_field(QString s)
{
  s.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
  s.replace(QLatin1Char('|'), QStringLiteral("\\|"));
  s.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
  return s;
}

QString unescape_field(QString s)
{
  QString out;
  out.reserve(s.size());
  for (int i = 0; i < s.size(); ++i) {
    if (s[i] == QLatin1Char('\\') && i + 1 < s.size()) {
      const QChar n = s[i + 1];
      if (n == QLatin1Char('n')) {
        out += QLatin1Char('\n');
      } else if (n == QLatin1Char('|') || n == QLatin1Char('\\')) {
        out += n;
      } else {
        out += n;
      }
      ++i;
      continue;
    }
    out += s[i];
  }
  return out;
}

QStringList split_fields(const QString& line)
{
  QStringList fields;
  QString cur;
  for (int i = 0; i < line.size(); ++i) {
    if (line[i] == QLatin1Char('\\') && i + 1 < line.size()) {
      cur += line[i];
      cur += line[i + 1];
      ++i;
      continue;
    }
    if (line[i] == QLatin1Char('|')) {
      fields << unescape_field(cur);
      cur.clear();
      continue;
    }
    cur += line[i];
  }
  fields << unescape_field(cur);
  return fields;
}

QString files_label(const QStringList& paths)
{
  if (paths.isEmpty()) {
    return QStringLiteral("(no files)");
  }
  if (paths.size() == 1) {
    return QFileInfo(paths.front()).fileName();
  }
  return QStringLiteral("%1 (%2 files)")
      .arg(QFileInfo(paths.front()).fileName())
      .arg(paths.size());
}

QIcon app_icon_for(const OpenHistoryEntry& e)
{
  if (!e.app_icon.isEmpty()) {
    const QIcon ic = QIcon::fromTheme(e.app_icon);
    if (!ic.isNull()) {
      return ic;
    }
  }
  return QIcon::fromTheme(QStringLiteral("system-run"),
                          QIcon::fromTheme(QStringLiteral("application-x-executable")));
}

QIcon file_icon_for(const QString& path)
{
  QFileIconProvider provider;
  const QFileInfo fi(path);
  if (fi.isDir()) {
    return provider.icon(QFileIconProvider::Folder);
  }
  return provider.icon(fi);
}

void reopen_entry(const OpenHistoryEntry& e)
{
  std::vector<std::filesystem::path> paths;
  paths.reserve(static_cast<std::size_t>(e.paths.size()));
  for (const QString& p : e.paths) {
    paths.emplace_back(p.toStdString());
  }
  if (paths.empty()) {
    return;
  }
  if (e.app_id.startsWith(QLatin1String("command:"))) {
    const QString cmd = e.app_id.mid(8);
    QStringList parts = cmd.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
      return;
    }
    QStringList args = parts.mid(1);
    for (const auto& p : paths) {
      args << QString::fromStdString(p.string());
    }
    QProcess::startDetached(parts.front(), args);
    return;
  }
  if (e.app_id == QLatin1String("default") || e.app_id.isEmpty()) {
    for (const auto& p : paths) {
      open_default(p);
    }
    return;
  }
  // Prefer launching by desktop id if we still have exec info via defaults lookup.
  DesktopApp app;
  app.id = e.app_id;
  app.name = e.app_name;
  app.icon = e.app_icon;
  // Resolve exec from .desktop again.
  const auto defaults = default_apps_for_paths(paths);
  for (const DesktopApp& a : defaults) {
    if (a.id == e.app_id) {
      launch_desktop_app(a, paths);
      return;
    }
  }
  const auto all = associated_apps_for_paths(paths);
  for (const DesktopApp& a : all) {
    if (a.id == e.app_id) {
      launch_desktop_app(a, paths);
      return;
    }
  }
  // Fall back to default handler(s).
  for (const auto& p : paths) {
    open_default(p);
  }
}

} // namespace

OpenHistory::OpenHistory(std::filesystem::path file)
    : path_(std::move(file))
{
  load();
}

std::filesystem::path OpenHistory::default_path()
{
  const QString cfg = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  std::filesystem::path dir = cfg.toStdString();
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir / "open_history.txt";
}

OpenHistory& open_history()
{
  static OpenHistory instance(OpenHistory::default_path());
  return instance;
}

void OpenHistory::load()
{
  entries_.clear();
  std::ifstream in(path_);
  if (!in) {
    return;
  }
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const QString qline = QString::fromStdString(line);
    const QStringList fields = split_fields(qline);
    if (fields.size() < 5) {
      continue;
    }
    OpenHistoryEntry e;
    e.when = QDateTime::fromString(fields[0], Qt::ISODate);
    if (!e.when.isValid()) {
      e.when = QDateTime::fromSecsSinceEpoch(fields[0].toLongLong());
    }
    e.app_id = fields[1];
    e.app_name = fields[2];
    e.app_icon = fields[3];
    for (int i = 4; i < fields.size(); ++i) {
      if (!fields[i].isEmpty()) {
        e.paths << fields[i];
      }
    }
    if (!e.paths.isEmpty()) {
      entries_.push_back(std::move(e));
    }
  }
}

void OpenHistory::save() const
{
  if (auto parent = path_.parent_path(); !parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
  }
  std::ofstream out(path_, std::ios::trunc);
  out << "# dirtoo open history: when|app_id|app_name|app_icon|path…\n";
  for (const OpenHistoryEntry& e : entries_) {
    out << escape_field(e.when.toString(Qt::ISODate)).toStdString() << '|'
        << escape_field(e.app_id).toStdString() << '|' << escape_field(e.app_name).toStdString()
        << '|' << escape_field(e.app_icon).toStdString();
    for (const QString& p : e.paths) {
      out << '|' << escape_field(p).toStdString();
    }
    out << '\n';
  }
}

void OpenHistory::record(OpenHistoryEntry entry)
{
  if (entry.paths.isEmpty()) {
    return;
  }
  if (!entry.when.isValid()) {
    entry.when = QDateTime::currentDateTime();
  }
  // De-dupe consecutive identical opens (same app + same paths).
  if (!entries_.empty()) {
    const OpenHistoryEntry& last = entries_.back();
    if (last.app_id == entry.app_id && last.paths == entry.paths) {
      entries_.back().when = entry.when;
      save();
      return;
    }
  }
  entries_.push_back(std::move(entry));
  while (static_cast<int>(entries_.size()) > kMaxEntries) {
    entries_.erase(entries_.begin());
  }
  save();
}

void OpenHistory::record_open(const QString& app_id, const QString& app_name, const QString& app_icon,
                              const std::vector<std::filesystem::path>& paths)
{
  if (paths.empty()) {
    return;
  }
  OpenHistoryEntry e;
  e.when = QDateTime::currentDateTime();
  e.app_id = app_id;
  e.app_name = app_name;
  e.app_icon = app_icon;
  for (const auto& p : paths) {
    e.paths << QString::fromStdString(p.string());
  }
  record(std::move(e));
}

std::vector<OpenHistoryEntry> OpenHistory::entries() const
{
  return entries_;
}

void OpenHistory::clear()
{
  entries_.clear();
  save();
}

void populate_recent_opens_menu(QMenu* menu, int limit)
{
  if (menu == nullptr) {
    return;
  }
  menu->clear();
  const auto all = open_history().entries();
  if (all.empty()) {
    auto* empty = menu->addAction(QStringLiteral("(no recent opens)"));
    empty->setEnabled(false);
    return;
  }
  int n = 0;
  for (auto it = all.rbegin(); it != all.rend() && n < limit; ++it, ++n) {
    const OpenHistoryEntry e = *it;
    const QString label =
        QStringLiteral("%1 — %2").arg(e.app_name.isEmpty() ? QStringLiteral("App") : e.app_name,
                                      files_label(e.paths));
    auto* act = menu->addAction(app_icon_for(e), label);
    act->setToolTip(e.paths.join(QStringLiteral("\n")));
    QObject::connect(act, &QAction::triggered, menu, [e] { reopen_entry(e); });
  }
}

void show_open_history_dialog(QWidget* parent,
                              std::function<void(const QString& directory)> on_go_to_folder)
{
  auto* dialog = new QDialog(parent);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(QStringLiteral("Open History"));
  dialog->resize(720, 420);

  auto* layout = new QVBoxLayout(dialog);
  auto* filter = new QLineEdit(dialog);
  filter->setPlaceholderText(QStringLiteral("Filter by app or path…"));
  filter->setClearButtonEnabled(true);
  layout->addWidget(filter);

  auto* tree = new QTreeWidget(dialog);
  tree->setColumnCount(3);
  tree->setHeaderLabels({QStringLiteral("When"), QStringLiteral("Application"),
                         QStringLiteral("Files")});
  tree->setRootIsDecorated(true);
  tree->setAlternatingRowColors(true);
  tree->setSelectionMode(QAbstractItemView::SingleSelection);
  tree->setUniformRowHeights(true);
  tree->header()->setStretchLastSection(true);
  tree->header()->resizeSection(0, 150);
  tree->header()->resizeSection(1, 160);
  layout->addWidget(tree, 1);

  const auto refill = [tree, filter] {
    tree->clear();
    const QString needle = filter->text().trimmed().toLower();
    const auto all = open_history().entries();
    for (auto it = all.rbegin(); it != all.rend(); ++it) {
      const OpenHistoryEntry& e = *it;
      if (!needle.isEmpty()) {
        bool hit = e.app_name.toLower().contains(needle) || e.app_id.toLower().contains(needle);
        if (!hit) {
          for (const QString& p : e.paths) {
            if (p.toLower().contains(needle)) {
              hit = true;
              break;
            }
          }
        }
        if (!hit) {
          continue;
        }
      }
      auto* row = new QTreeWidgetItem(tree);
      row->setText(0, e.when.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
      row->setText(1, e.app_name.isEmpty() ? e.app_id : e.app_name);
      row->setIcon(1, app_icon_for(e));
      row->setText(2, files_label(e.paths));
      row->setToolTip(2, e.paths.join(QLatin1Char('\n')));
      row->setData(0, Qt::UserRole, e.app_id);
      row->setData(1, Qt::UserRole, e.app_name);
      row->setData(2, Qt::UserRole, e.app_icon);
      row->setData(0, Qt::UserRole + 1, e.paths);
      for (const QString& p : e.paths) {
        auto* child = new QTreeWidgetItem(row);
        child->setText(2, p);
        child->setIcon(2, file_icon_for(p));
        child->setToolTip(2, p);
      }
    }
  };
  QObject::connect(filter, &QLineEdit::textChanged, dialog, refill);
  refill();

  auto* buttons = new QDialogButtonBox(dialog);
  auto* reopen_btn = buttons->addButton(QStringLiteral("Re-open"), QDialogButtonBox::ActionRole);
  auto* folder_btn =
      buttons->addButton(QStringLiteral("Go to Folder"), QDialogButtonBox::ActionRole);
  auto* clear_btn = buttons->addButton(QStringLiteral("Clear History"), QDialogButtonBox::ActionRole);
  buttons->addButton(QDialogButtonBox::Close);
  layout->addWidget(buttons);

  const auto selected_entry = [tree]() -> std::optional<OpenHistoryEntry> {
    auto* item = tree->currentItem();
    if (item == nullptr) {
      return std::nullopt;
    }
    if (item->parent() != nullptr) {
      item = item->parent();
    }
    OpenHistoryEntry e;
    e.app_id = item->data(0, Qt::UserRole).toString();
    e.app_name = item->data(1, Qt::UserRole).toString();
    e.app_icon = item->data(2, Qt::UserRole).toString();
    e.paths = item->data(0, Qt::UserRole + 1).toStringList();
    e.when = QDateTime::fromString(item->text(0), QStringLiteral("yyyy-MM-dd HH:mm"));
    if (e.paths.isEmpty()) {
      return std::nullopt;
    }
    return e;
  };

  QObject::connect(reopen_btn, &QPushButton::clicked, dialog, [selected_entry] {
    if (const auto e = selected_entry()) {
      reopen_entry(*e);
    }
  });
  QObject::connect(tree, &QTreeWidget::itemDoubleClicked, dialog,
                   [selected_entry](QTreeWidgetItem*, int) {
                     if (const auto e = selected_entry()) {
                       reopen_entry(*e);
                     }
                   });
  QObject::connect(folder_btn, &QPushButton::clicked, dialog,
                   [selected_entry, on_go_to_folder] {
                     const auto e = selected_entry();
                     if (!e || e->paths.isEmpty()) {
                       return;
                     }
                     const QFileInfo fi(e->paths.front());
                     const QString dir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
                     if (on_go_to_folder) {
                       on_go_to_folder(dir);
                     } else {
                       open_default(std::filesystem::path{dir.toStdString()});
                     }
                   });
  QObject::connect(clear_btn, &QPushButton::clicked, dialog, [refill, dialog] {
    if (QMessageBox::question(dialog, QStringLiteral("Clear Open History"),
                              QStringLiteral("Remove all recorded file opens?"))
        == QMessageBox::Yes) {
      open_history().clear();
      refill();
    }
  });
  QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

  dialog->show();
}

} // namespace dirtoo::app

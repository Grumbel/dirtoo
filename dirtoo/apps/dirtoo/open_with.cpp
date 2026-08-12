// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "open_with.hpp"

#include "open_history.hpp"

#include <QAction>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMimeDatabase>
#include <QProcess>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

#include <algorithm>

namespace dirtoo::app {
namespace {

QStringList data_dirs()
{
  QStringList dirs;
  const QString home = QDir::homePath();
  dirs << (home + QStringLiteral("/.local/share"));
  const QByteArray xdg = qgetenv("XDG_DATA_DIRS");
  if (!xdg.isEmpty()) {
    for (const QByteArray& part : xdg.split(':')) {
      if (!part.isEmpty()) {
        dirs << QString::fromLocal8Bit(part);
      }
    }
  } else {
    dirs << QStringLiteral("/usr/local/share") << QStringLiteral("/usr/share");
  }
  return dirs;
}

QStringList mimeapps_paths()
{
  QStringList out;
  const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
  out << (config + QStringLiteral("/mimeapps.list"));
  out << (QDir::homePath() + QStringLiteral("/.config/mimeapps.list"));
  for (const QString& d : data_dirs()) {
    out << (d + QStringLiteral("/applications/mimeapps.list"));
    out << (d + QStringLiteral("/applications/mimeinfo.cache"));
  }
  return out;
}

QString find_desktop_file(const QString& id)
{
  if (id.isEmpty()) {
    return {};
  }
  if (QFileInfo::exists(id) && id.endsWith(QLatin1String(".desktop"))) {
    return id;
  }
  const QString name = id.endsWith(QLatin1String(".desktop")) ? id : (id + QStringLiteral(".desktop"));
  for (const QString& d : data_dirs()) {
    const QString path = d + QStringLiteral("/applications/") + name;
    if (QFileInfo::exists(path)) {
      return path;
    }
  }
  return {};
}

DesktopApp parse_desktop_file(const QString& path)
{
  DesktopApp app;
  if (path.isEmpty()) {
    return app;
  }
  QSettings ini(path, QSettings::IniFormat);
  ini.beginGroup(QStringLiteral("Desktop Entry"));
  if (ini.value(QStringLiteral("NoDisplay")).toBool() || ini.value(QStringLiteral("Hidden")).toBool()) {
    return app;
  }
  app.id = QFileInfo(path).fileName();
  app.name = ini.value(QStringLiteral("Name")).toString();
  app.exec = ini.value(QStringLiteral("Exec")).toString();
  app.icon = ini.value(QStringLiteral("Icon")).toString();
  ini.endGroup();
  return app;
}

QStringList desktop_ids_for_mime_from_file(const QString& path, const QString& mime,
                                           const QString& section)
{
  QStringList ids;
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return ids;
  }
  QTextStream in(&f);
  QString current;
  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
      current = line.mid(1, line.size() - 2);
      continue;
    }
    if (current.compare(section, Qt::CaseInsensitive) != 0) {
      continue;
    }
    const int eq = line.indexOf(QLatin1Char('='));
    if (eq <= 0) {
      continue;
    }
    const QString key = line.left(eq).trimmed();
    if (key != mime) {
      continue;
    }
    const QString val = line.mid(eq + 1).trimmed();
    for (const QString& part : val.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
      const QString id = part.trimmed();
      if (!id.isEmpty() && !ids.contains(id)) {
        ids << id;
      }
    }
  }
  return ids;
}

QStringList desktop_ids_from_sections(const QString& mime, const QStringList& sections)
{
  QStringList ids;
  for (const QString& path : mimeapps_paths()) {
    for (const QString& section : sections) {
      for (const QString& id : desktop_ids_for_mime_from_file(path, mime, section)) {
        if (!ids.contains(id)) {
          ids << id;
        }
      }
    }
  }
  return ids;
}

/// Apps that declare MimeType= including this type in their .desktop file.
QStringList desktop_ids_from_desktop_mimetypes(const QString& mime)
{
  QStringList ids;
  QSet<QString> seen;
  for (const QString& d : data_dirs()) {
    const QDir app_dir(d + QStringLiteral("/applications"));
    if (!app_dir.exists()) {
      continue;
    }
    const QStringList files =
        app_dir.entryList(QStringList{QStringLiteral("*.desktop")}, QDir::Files);
    for (const QString& name : files) {
      const QString path = app_dir.filePath(name);
      QSettings ini(path, QSettings::IniFormat);
      ini.beginGroup(QStringLiteral("Desktop Entry"));
      if (ini.value(QStringLiteral("NoDisplay")).toBool()
          || ini.value(QStringLiteral("Hidden")).toBool()) {
        ini.endGroup();
        continue;
      }
      const QString mime_line = ini.value(QStringLiteral("MimeType")).toString();
      ini.endGroup();
      if (mime_line.isEmpty()) {
        continue;
      }
      const QStringList declared = mime_line.split(QLatin1Char(';'), Qt::SkipEmptyParts);
      for (QString m : declared) {
        if (m.trimmed() == mime && !seen.contains(name)) {
          seen.insert(name);
          ids << name;
          break;
        }
      }
    }
  }
  return ids;
}

std::vector<DesktopApp> apps_from_ids(const QStringList& ids)
{
  std::vector<DesktopApp> apps;
  QSet<QString> seen;
  for (const QString& id : ids) {
    const QString path = find_desktop_file(id);
    DesktopApp app = parse_desktop_file(path);
    if (app.name.isEmpty() || app.exec.isEmpty()) {
      continue;
    }
    if (seen.contains(app.id)) {
      continue;
    }
    seen.insert(app.id);
    apps.push_back(std::move(app));
  }
  std::sort(apps.begin(), apps.end(),
            [](const DesktopApp& a, const DesktopApp& b) { return a.name.toLower() < b.name.toLower(); });
  return apps;
}

QStringList expand_exec(const QString& exec, const std::vector<std::filesystem::path>& paths)
{
  QStringList parts = QProcess::splitCommand(exec);
  if (parts.isEmpty()) {
    return parts;
  }
  QStringList out;
  bool used_file_code = false;
  for (QString p : parts) {
    if (p == QLatin1String("%f") || p == QLatin1String("%u")) {
      if (!paths.empty()) {
        out << QString::fromStdString(paths.front().string());
      }
      used_file_code = true;
    } else if (p == QLatin1String("%F") || p == QLatin1String("%U")) {
      for (const auto& path : paths) {
        out << QString::fromStdString(path.string());
      }
      used_file_code = true;
    } else if (p == QLatin1String("%i") || p == QLatin1String("%c") || p == QLatin1String("%k")) {
      // ignore icon/name/desktop-file codes
    } else {
      p.replace(QStringLiteral("%f"), QString());
      p.replace(QStringLiteral("%F"), QString());
      p.replace(QStringLiteral("%u"), QString());
      p.replace(QStringLiteral("%U"), QString());
      if (!p.isEmpty()) {
        out << p;
      }
    }
  }
  if (!used_file_code) {
    for (const auto& path : paths) {
      out << QString::fromStdString(path.string());
    }
  }
  return out;
}

QString mime_for_path(const std::filesystem::path& p)
{
  // Extension-only: MatchContent reads file bytes and freezes the UI when the
  // context menu walks a large multi-selection.
  QMimeDatabase db;
  const QMimeType mt =
      db.mimeTypeForFile(QString::fromStdString(p.string()), QMimeDatabase::MatchExtension);
  return mt.isValid() ? mt.name() : QStringLiteral("application/octet-stream");
}

/// Cap how many paths we inspect for MIME/app intersection. Beyond this we only
/// sample; common case is select-all of one type (unique MIME stays small).
constexpr std::size_t kOpenWithPathSample = 64;

[[nodiscard]] std::vector<QString> unique_mimes_for_paths(const std::vector<std::filesystem::path>& paths)
{
  QSet<QString> seen;
  std::vector<QString> out;
  const std::size_t n = std::min(paths.size(), kOpenWithPathSample);
  out.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    const QString mime = mime_for_path(paths[i]);
    if (seen.contains(mime)) {
      continue;
    }
    seen.insert(mime);
    out.push_back(mime);
  }
  return out;
}

std::vector<DesktopApp> intersect_apps(const std::vector<std::vector<DesktopApp>>& sets)
{
  if (sets.empty()) {
    return {};
  }
  std::vector<DesktopApp> common = sets.front();
  for (std::size_t i = 1; i < sets.size(); ++i) {
    std::vector<DesktopApp> next;
    for (const DesktopApp& a : common) {
      for (const DesktopApp& b : sets[i]) {
        if (a.id == b.id) {
          next.push_back(a);
          break;
        }
      }
    }
    common = std::move(next);
  }
  return common;
}

} // namespace

bool open_default(const std::filesystem::path& path)
{
  const bool ok =
      QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path.string())));
  if (ok) {
    QString app_id = QStringLiteral("default");
    QString app_name = QStringLiteral("Default application");
    QString app_icon;
    const auto defaults = default_apps_for_paths({path});
    if (!defaults.empty()) {
      app_id = defaults.front().id;
      app_name = defaults.front().name;
      app_icon = defaults.front().icon;
    }
    open_history().record_open(app_id, app_name, app_icon, {path});
  }
  return ok;
}

bool open_in_terminal(const std::filesystem::path& directory)
{
  const QString dir = QString::fromStdString(directory.string());
  const QByteArray env_term = qgetenv("TERMINAL");
  QStringList candidates;
  if (!env_term.isEmpty()) {
    candidates << QString::fromLocal8Bit(env_term);
  }
  candidates << QStringLiteral("x-terminal-emulator") << QStringLiteral("kitty")
             << QStringLiteral("alacritty") << QStringLiteral("gnome-terminal")
             << QStringLiteral("konsole") << QStringLiteral("xfce4-terminal")
             << QStringLiteral("xterm");

  for (const QString& term : candidates) {
    QStringList args;
    if (term == QLatin1String("xterm")) {
      args << QStringLiteral("-e") << QStringLiteral("bash") << QStringLiteral("-lc")
           << QStringLiteral("cd \"$1\" && exec bash") << QStringLiteral("--") << dir;
    } else if (term.contains(QLatin1String("gnome-terminal"))) {
      args << QStringLiteral("--working-directory") << dir;
    } else if (term.contains(QLatin1String("konsole"))) {
      args << QStringLiteral("--workdir") << dir;
    } else {
      args << QStringLiteral("--working-directory") << dir;
    }
    if (QProcess::startDetached(term, args)) {
      return true;
    }
  }
  return false;
}

bool open_with_command_dialog(QWidget* parent, const std::vector<std::filesystem::path>& paths)
{
  if (paths.empty()) {
    return false;
  }
  bool ok = false;
  const QString cmd = QInputDialog::getText(
      parent, QStringLiteral("Open with"),
      QStringLiteral("Command (paths are appended as arguments):"), QLineEdit::Normal,
      QStringLiteral("xdg-open"), &ok);
  if (!ok || cmd.trimmed().isEmpty()) {
    return false;
  }

  QStringList args;
  for (const auto& p : paths) {
    args << QString::fromStdString(p.string());
  }

  const QStringList parts = cmd.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
  if (parts.isEmpty()) {
    return false;
  }
  const QString program = parts.front();
  QStringList full_args = parts.mid(1);
  full_args.append(args);
  const bool launched = QProcess::startDetached(program, full_args);
  if (launched) {
    open_history().record_open(QStringLiteral("command:%1").arg(cmd.trimmed()), cmd.trimmed(),
                               QStringLiteral("system-run"), paths);
  }
  return launched;
}

std::vector<DesktopApp> default_apps_for_mime(const QString& mime_type)
{
  return apps_from_ids(
      desktop_ids_from_sections(mime_type, {QStringLiteral("Default Applications")}));
}

std::vector<DesktopApp> apps_for_mime(const QString& mime_type)
{
  QStringList ids = desktop_ids_from_sections(
      mime_type, {QStringLiteral("Default Applications"), QStringLiteral("Added Associations"),
                  QStringLiteral("MIME Cache")});
  for (const QString& id : desktop_ids_from_desktop_mimetypes(mime_type)) {
    if (!ids.contains(id)) {
      ids << id;
    }
  }
  return apps_from_ids(ids);
}

bool launch_desktop_app(const DesktopApp& app, const std::vector<std::filesystem::path>& paths)
{
  if (app.exec.isEmpty() || paths.empty()) {
    return false;
  }
  const QStringList argv = expand_exec(app.exec, paths);
  if (argv.isEmpty()) {
    return false;
  }
  const bool ok = QProcess::startDetached(argv.front(), argv.mid(1));
  if (ok) {
    open_history().record_open(app.id, app.name.isEmpty() ? app.id : app.name, app.icon, paths);
  }
  return ok;
}

std::vector<DesktopApp> default_apps_for_paths(const std::vector<std::filesystem::path>& paths)
{
  if (paths.empty()) {
    return {};
  }
  const auto mimes = unique_mimes_for_paths(paths);
  std::vector<std::vector<DesktopApp>> sets;
  sets.reserve(mimes.size());
  for (const QString& mime : mimes) {
    sets.push_back(default_apps_for_mime(mime));
  }
  return intersect_apps(sets);
}

std::vector<DesktopApp> associated_apps_for_paths(const std::vector<std::filesystem::path>& paths)
{
  if (paths.empty()) {
    return {};
  }
  const auto mimes = unique_mimes_for_paths(paths);
  std::vector<std::vector<DesktopApp>> sets;
  sets.reserve(mimes.size());
  for (const QString& mime : mimes) {
    sets.push_back(apps_for_mime(mime));
  }
  return intersect_apps(sets);
}

void add_default_open_actions(QMenu* menu, const std::vector<std::filesystem::path>& paths)
{
  if (menu == nullptr || paths.empty()) {
    return;
  }
  const auto defaults = default_apps_for_paths(paths);
  if (defaults.empty()) {
    auto* none = menu->addAction(QStringLiteral("No applications available"));
    none->setEnabled(false);
    return;
  }
  for (const DesktopApp& app : defaults) {
    QAction* act =
        menu->addAction(QStringLiteral("Open With %1").arg(app.name));
    if (!app.icon.isEmpty()) {
      act->setIcon(QIcon::fromTheme(app.icon));
    }
    const DesktopApp captured = app;
    QObject::connect(act, &QAction::triggered, menu, [captured, paths] {
      launch_desktop_app(captured, paths);
    });
  }
}

void populate_open_with_menu(QMenu* menu, const std::vector<std::filesystem::path>& paths)
{
  if (menu == nullptr || paths.empty()) {
    return;
  }
  // Defer MIME/desktop scanning until the submenu is opened so a right-click on
  // a huge selection stays responsive (defaults still built on first show).
  // Note: do not use Qt::UniqueConnection with a lambda — Qt does not support
  // UniqueConnection for functors, so the connect can fail and leave only "…".
  menu->clear();
  auto* placeholder = menu->addAction(QStringLiteral("…"));
  placeholder->setEnabled(false);

  QObject::connect(menu, &QMenu::aboutToShow, menu, [menu, paths] {
    if (menu->property("dirtoo_open_with_filled").toBool()) {
      return;
    }
    menu->setProperty("dirtoo_open_with_filled", true);
    menu->clear();

    const auto defaults = default_apps_for_paths(paths);
    QSet<QString> default_ids;
    for (const DesktopApp& a : defaults) {
      default_ids.insert(a.id);
    }
    const auto all = associated_apps_for_paths(paths);
    std::vector<DesktopApp> others;
    for (const DesktopApp& a : all) {
      if (!default_ids.contains(a.id)) {
        others.push_back(a);
      }
    }

    if (others.empty() && defaults.empty()) {
      auto* none = menu->addAction(QStringLiteral("No applications available"));
      none->setEnabled(false);
    }

    for (const DesktopApp& app : others) {
      QAction* act = menu->addAction(app.name);
      if (!app.icon.isEmpty()) {
        act->setIcon(QIcon::fromTheme(app.icon));
      }
      const DesktopApp captured = app;
      QObject::connect(act, &QAction::triggered, menu, [captured, paths] {
        launch_desktop_app(captured, paths);
      });
    }

    if (!others.empty()) {
      menu->addSeparator();
    }
    QAction* other = menu->addAction(QStringLiteral("Other Application…"));
    QObject::connect(other, &QAction::triggered, menu, [menu, paths] {
      open_with_command_dialog(menu->parentWidget(), paths);
    });
  });
}

} // namespace dirtoo::app

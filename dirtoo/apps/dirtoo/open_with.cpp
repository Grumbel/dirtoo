// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "open_with.hpp"

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

QStringList desktop_ids_for_mime(const QString& mime)
{
  QStringList ids;
  for (const QString& path : mimeapps_paths()) {
    for (const QString& section :
         {QStringLiteral("Default Applications"), QStringLiteral("Added Associations"),
          QStringLiteral("MIME Cache")}) {
      for (const QString& id : desktop_ids_for_mime_from_file(path, mime, section)) {
        if (!ids.contains(id)) {
          ids << id;
        }
      }
    }
  }
  return ids;
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
      // strip remaining % codes inside tokens
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

} // namespace

bool open_default(const std::filesystem::path& path)
{
  return QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path.string())));
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
  return QProcess::startDetached(program, full_args);
}

std::vector<DesktopApp> apps_for_mime(const QString& mime_type)
{
  std::vector<DesktopApp> apps;
  QSet<QString> seen;
  for (const QString& id : desktop_ids_for_mime(mime_type)) {
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
  return apps;
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
  return QProcess::startDetached(argv.front(), argv.mid(1));
}

void populate_open_with_menu(QMenu* menu, const std::vector<std::filesystem::path>& paths)
{
  if (menu == nullptr || paths.empty()) {
    return;
  }
  QMimeDatabase db;
  QSet<QString> mimes;
  for (const auto& p : paths) {
    const QMimeType mt = db.mimeTypeForFile(QString::fromStdString(p.string()),
                                            QMimeDatabase::MatchExtension);
    mimes.insert(mt.isValid() ? mt.name() : QStringLiteral("application/octet-stream"));
  }

  // Intersection of apps across all selected MIME types.
  std::vector<DesktopApp> common;
  bool first = true;
  for (const QString& mime : mimes) {
    const auto apps = apps_for_mime(mime);
    if (first) {
      common = apps;
      first = false;
    } else {
      std::vector<DesktopApp> next;
      for (const DesktopApp& a : common) {
        for (const DesktopApp& b : apps) {
          if (a.id == b.id) {
            next.push_back(a);
            break;
          }
        }
      }
      common = std::move(next);
    }
  }

  for (const DesktopApp& app : common) {
    QAction* act = menu->addAction(app.name);
    if (!app.icon.isEmpty()) {
      act->setIcon(QIcon::fromTheme(app.icon));
    }
    const DesktopApp captured = app;
    QObject::connect(act, &QAction::triggered, menu, [captured, paths] {
      launch_desktop_app(captured, paths);
    });
  }

  if (!common.empty()) {
    menu->addSeparator();
  }
  QAction* other = menu->addAction(QStringLiteral("Other Application…"));
  QObject::connect(other, &QAction::triggered, menu, [menu, paths] {
    open_with_command_dialog(menu->parentWidget(), paths);
  });
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "open_with.hpp"

#include <QDesktopServices>
#include <QInputDialog>
#include <QLineEdit>
#include <QProcess>
#include <QUrl>

namespace dirtoo::app {

bool open_default(const std::filesystem::path& path)
{
  return QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path.string())));
}

bool open_in_terminal(const std::filesystem::path& directory)
{
  const QString dir = QString::fromStdString(directory.string());
  // Prefer explicit env, then common emulators.
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
    // Most terminals accept --working-directory; xterm uses -e with cd.
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

  // Split command into program + optional fixed args (simple whitespace split).
  const QStringList parts = cmd.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
  if (parts.isEmpty()) {
    return false;
  }
  const QString program = parts.front();
  QStringList full_args = parts.mid(1);
  full_args.append(args);
  return QProcess::startDetached(program, full_args);
}

} // namespace dirtoo::app

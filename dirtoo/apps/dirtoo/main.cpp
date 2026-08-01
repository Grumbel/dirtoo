// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"

#include "app_settings.hpp"

#include "dirtoo/fs/location.hpp"

#include <QApplication>
#include <QIcon>
#include <QDir>

#include <filesystem>

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("dirtoo"));
  QApplication::setOrganizationName(QStringLiteral("dirtoo"));
  QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
  QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/dirtoo.png")));
  // Also try theme / installed icon name.
  if (QApplication::windowIcon().isNull()) {
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("dirtoo"),
                                                 QIcon(QStringLiteral("/usr/share/icons/hicolor/48x48/apps/dirtoo.png"))));
  }

  dirtoo::app::MainWindow window;

  std::filesystem::path start = QDir::homePath().toStdString();
  if (argc > 1) {
    start = argv[1];
  } else {
    const auto settings = dirtoo::app::load_settings();
    if (!settings.last_location.isEmpty()) {
      start = settings.last_location.toStdString();
    }
  }
  window.open_location(dirtoo::fs::Location::from_path(start));
  window.show();

  return app.exec();
}

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"

#include "app_settings.hpp"

#include "dirtoo/fs/location.hpp"

#include <QApplication>
#include <QDir>

#include <filesystem>

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("dirtoo"));
  QApplication::setOrganizationName(QStringLiteral("dirtoo"));
  QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

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

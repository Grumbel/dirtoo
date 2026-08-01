// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"

#include "app_settings.hpp"

#include "dirtoo/fs/location.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QLoggingCategory>

#include <filesystem>
#include <iostream>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("dirtoo"));
  QApplication::setOrganizationName(QStringLiteral("dirtoo"));
  QApplication::setApplicationVersion(QStringLiteral(DIRTOO_VERSION));
  QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/dirtoo.png")));
  if (QApplication::windowIcon().isNull()) {
    QApplication::setWindowIcon(QIcon::fromTheme(
        QStringLiteral("dirtoo"),
        QIcon(QStringLiteral("/usr/share/icons/hicolor/48x48/apps/dirtoo.png"))));
  }

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("Modular Qt file manager"));
  parser.addHelpOption();
  parser.addVersionOption(); // --version / -v via Qt
  QCommandLineOption verbose_opt({QStringLiteral("verbose"), QStringLiteral("V")},
                                 QStringLiteral("Log regular events (info level)"));
  QCommandLineOption debug_opt(QStringLiteral("debug"),
                               QStringLiteral("Log extreme debug messages"));
  parser.addOption(verbose_opt);
  parser.addOption(debug_opt);
  parser.addPositionalArgument(QStringLiteral("path"),
                               QStringLiteral("Initial directory to open"),
                               QStringLiteral("[path]"));
  parser.process(app);

  if (parser.isSet(debug_opt)) {
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true\nqt.*.debug=false"));
  } else if (parser.isSet(verbose_opt)) {
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false\n*.info=true"));
  } else {
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false\n*.info=false"));
  }

  dirtoo::app::MainWindow window;

  std::filesystem::path start = QDir::homePath().toStdString();
  const QStringList pos = parser.positionalArguments();
  if (!pos.isEmpty()) {
    start = pos.first().toStdString();
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

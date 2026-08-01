// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"

#include "app_settings.hpp"

#include "dirtoo/fs/location.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QIcon>
#include <QLoggingCategory>

#include <filesystem>
#include <iostream>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace {

QtMsgType g_min_level = QtWarningMsg;

void dirtoo_message_handler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
  if (type < g_min_level) {
    return;
  }
  const char* level = "???";
  switch (type) {
  case QtDebugMsg:
    level = "debug";
    break;
  case QtInfoMsg:
    level = "info";
    break;
  case QtWarningMsg:
    level = "warning";
    break;
  case QtCriticalMsg:
    level = "critical";
    break;
  case QtFatalMsg:
    level = "fatal";
    break;
  }
  const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
  std::cerr << stamp.toStdString() << " [" << level << "] ";
  if (context.category != nullptr && context.category[0] != '\0'
      && QLatin1String(context.category) != QLatin1String("default")) {
    std::cerr << context.category << ": ";
  }
  std::cerr << msg.toStdString() << '\n';
  if (type == QtFatalMsg) {
    std::abort();
  }
}

} // namespace

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
                                 QStringLiteral("Log regular events (info level) to stderr"));
  QCommandLineOption debug_opt(QStringLiteral("debug"),
                               QStringLiteral("Log debug messages to stderr"));
  parser.addOption(verbose_opt);
  parser.addOption(debug_opt);
  parser.addPositionalArgument(QStringLiteral("path"),
                               QStringLiteral("Initial directory to open"),
                               QStringLiteral("[path]"));
  parser.process(app);

  // Message handler first so filter rules + level gate both apply.
  qInstallMessageHandler(dirtoo_message_handler);

  if (parser.isSet(debug_opt)) {
    g_min_level = QtDebugMsg;
    // Enable app debug; keep noisy Qt modules quiet unless explicitly wanted.
    QLoggingCategory::setFilterRules(
        QStringLiteral("*.debug=true\nqt.*.debug=false\nqt.qpa.*.debug=false"));
  } else if (parser.isSet(verbose_opt)) {
    g_min_level = QtInfoMsg;
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false\n*.info=true"));
  } else {
    g_min_level = QtWarningMsg;
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false\n*.info=false"));
  }

  if (parser.isSet(debug_opt) || parser.isSet(verbose_opt)) {
    qInfo().noquote() << QStringLiteral("dirtoo %1 starting (verbose=%2 debug=%3)")
                             .arg(QStringLiteral(DIRTOO_VERSION))
                             .arg(parser.isSet(verbose_opt))
                             .arg(parser.isSet(debug_opt));
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

  if (parser.isSet(debug_opt) || parser.isSet(verbose_opt)) {
    qInfo().noquote() << QStringLiteral("opening %1").arg(QString::fromStdString(start.string()));
  }

  window.open_location(dirtoo::fs::Location::from_path(start));
  window.show();

  return app.exec();
}

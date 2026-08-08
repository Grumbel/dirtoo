// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"
#include "badge_icons.hpp"

#include "app_settings.hpp"

#include "dirtoo/fs/location.hpp"

#include <QApplication>
#include <QObject>
#include <QDialog>
#include <QDialogButtonBox>
#include <QProxyStyle>
#include <QStyle>
#include <QStyleFactory>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QIcon>
#include <QPixmap>
#include <QLoggingCategory>

#include <filesystem>
#include <iostream>
#include <string_view>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace {

QtMsgType g_min_level = QtWarningMsg;

const char* path_basename(const char* path)
{
  if (path == nullptr || path[0] == '\0') {
    return nullptr;
  }
  const char* slash = path;
  for (const char* p = path; *p != '\0'; ++p) {
    if (*p == '/' || *p == '\\') {
      slash = p + 1;
    }
  }
  return slash;
}

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

  // Source location (requires QT_MESSAGELOGCONTEXT at compile time).
  const char* file = path_basename(context.file);
  if (file != nullptr && file[0] != '\0') {
    std::cerr << file;
    if (context.line > 0) {
      std::cerr << ':' << context.line;
    }
    std::cerr << ' ';
  }
  if (context.function != nullptr && context.function[0] != '\0') {
    // Prefer a short function token: strip return type if present is hard;
    // print the full signature Qt provides, truncated for readability.
    std::string_view fn{context.function};
    if (fn.size() > 96) {
      std::cerr.write(fn.data(), 93);
      std::cerr << "...";
    } else {
      std::cerr << context.function;
    }
    std::cerr << ": ";
  } else if (context.category != nullptr && context.category[0] != '\0'
             && QLatin1String(context.category) != QLatin1String("default")) {
    std::cerr << context.category << ": ";
  }

  std::cerr << msg.toStdString() << '\n';
  if (type == QtFatalMsg) {
    std::abort();
  }
}


/// Force GNOME dialog button order: [Cancel] … [OK] (affirmative on the right).
class GnomeButtonOrderStyle : public QProxyStyle {
public:
  explicit GnomeButtonOrderStyle(QStyle* base = nullptr)
      : QProxyStyle(base)
  {
  }

  int styleHint(StyleHint hint, const QStyleOption* option = nullptr,
                const QWidget* widget = nullptr,
                QStyleHintReturn* returnData = nullptr) const override
  {
    if (hint == QStyle::SH_DialogButtonLayout) {
      return QDialogButtonBox::GnomeLayout;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
  }
};

} // namespace

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  // GNOME-style dialog buttons: Cancel to the left of OK (and similar pairs).
  {
    const QString base_name = app.style() != nullptr ? app.style()->name() : QString();
    QStyle* base = base_name.isEmpty() ? nullptr : QStyleFactory::create(base_name);
    app.setStyle(new GnomeButtonOrderStyle(base));
  }
  QApplication::setApplicationName(QStringLiteral("dirtoo"));
  QApplication::setOrganizationName(QStringLiteral("dirtoo"));
  QApplication::setApplicationVersion(QStringLiteral(DIRTOO_VERSION));
  {
    const QPixmap app_pm = dirtoo::app::load_badge_pixmap(QStringLiteral("dirtoo.png"));
    if (!app_pm.isNull()) {
      QApplication::setWindowIcon(QIcon(app_pm));
    } else {
      QApplication::setWindowIcon(QIcon::fromTheme(
          QStringLiteral("dirtoo"),
          QIcon(QStringLiteral("/usr/share/icons/hicolor/48x48/apps/dirtoo.png"))));
    }
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

  // Always preflight badge assets so missing icons are visible on stderr without
  // requiring --verbose/--debug (load_badge_pixmap itself logs failures).
  {
    const QString idir = dirtoo::app::icon_directory();
    if (idir.isEmpty()) {
      qWarning().noquote() << QStringLiteral(
          "dirtoo: icon directory not resolved; using embedded :/icons/ only");
    }
    for (const char* name :
         {"badge-image.png", "badge-video.png", "badge-new.png", "badge-loading.png",
          "badge-error.png", "badge-locked.png", "badge-readonly.png", "badge-nowrite.png",
          "dirtoo.png"}) {
      const QPixmap pm = dirtoo::app::load_badge_pixmap(QLatin1String(name));
      if (pm.isNull()) {
        qWarning().noquote() << QStringLiteral("dirtoo: required icon failed to load: %1")
                                    .arg(QLatin1String(name));
      } else if (parser.isSet(debug_opt) || parser.isSet(verbose_opt)) {
        qInfo().noquote() << QStringLiteral("icon %1 ok %2x%3")
                                 .arg(QLatin1String(name))
                                 .arg(pm.width())
                                 .arg(pm.height());
      }
    }
  }

  if (parser.isSet(debug_opt) || parser.isSet(verbose_opt)) {
    qInfo().noquote() << QStringLiteral("dirtoo %1 starting (verbose=%2 debug=%3)")
                             .arg(QStringLiteral(DIRTOO_VERSION))
                             .arg(parser.isSet(verbose_opt))
                             .arg(parser.isSet(debug_opt));
    qInfo().noquote() << QStringLiteral("icon dir: %1")
                             .arg(dirtoo::app::icon_directory().isEmpty()
                                      ? QStringLiteral("(not found)")
                                      : dirtoo::app::icon_directory());
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

  // Under-development warning (dismissible permanently via settings).
  {
    dirtoo::app::AppSettings warn_settings = dirtoo::app::load_settings();
    if (!warn_settings.dismiss_dev_warning) {
      QDialog warn(&window);
      warn.setWindowTitle(QStringLiteral("dirtoo — Under Development"));
      warn.setModal(true);
      auto* layout = new QVBoxLayout(&warn);
      auto* icon_row = new QHBoxLayout();
      auto* icon = new QLabel(&warn);
      const QIcon themed = QIcon::fromTheme(QStringLiteral("dialog-warning"));
      if (!themed.isNull()) {
        icon->setPixmap(themed.pixmap(48, 48));
      }
      icon_row->addWidget(icon, 0, Qt::AlignTop);
      auto* msg = new QLabel(
          QStringLiteral(
              "<b>This application is under active development.</b><br><br>"
              "dirtoo is a work-in-progress file manager. Features may be incomplete, "
              "unstable, or destructive. <b>Do not use it to access or modify important "
              "data</b> you cannot afford to lose.<br><br>"
              "Prefer a mature file manager for production work."),
          &warn);
      msg->setWordWrap(true);
      msg->setTextFormat(Qt::RichText);
      icon_row->addWidget(msg, 1);
      layout->addLayout(icon_row);
      auto* dismiss = new QCheckBox(QStringLiteral("Do not show this warning again"), &warn);
      layout->addWidget(dismiss);
      auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &warn);
      QObject::connect(buttons, &QDialogButtonBox::accepted, &warn, &QDialog::accept);
      layout->addWidget(buttons);
      warn.resize(480, 220);
      warn.exec();
      if (dismiss->isChecked()) {
        warn_settings.dismiss_dev_warning = true;
        dirtoo::app::save_settings(warn_settings);
      }
    }
  }

  window.show();

  return app.exec();
}

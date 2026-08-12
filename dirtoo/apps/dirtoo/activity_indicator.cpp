// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "activity_indicator.hpp"

#include "activity_dialog.hpp"
#include "activity_monitor.hpp"
#include "theme_icons.hpp"

#include <QFontMetrics>

namespace dirtoo::app {

ActivityIndicator::ActivityIndicator(QWidget* parent)
    : QToolButton(parent)
{
  setAutoRaise(true);
  setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  setIcon(theme_icon("view-refresh", "process-working"));
  setText(QStringLiteral("Idle"));
  setToolTip(QStringLiteral("Background activity — click for details and log"));
  setMinimumWidth(QFontMetrics(font()).horizontalAdvance(QStringLiteral("Idle")) + 36);

  connect(this, &QToolButton::clicked, this, &ActivityIndicator::open_dialog);
  connect(&ActivityMonitor::instance(), &ActivityMonitor::changed, this, &ActivityIndicator::refresh);
  refresh();
}

void ActivityIndicator::refresh()
{
  auto& mon = ActivityMonitor::instance();
  const bool busy = mon.any_active();
  setText(mon.headline());
  if (busy) {
    setIcon(theme_icon("process-working", "view-refresh"));
    QString tip = QStringLiteral("Background activity:\n");
    for (const auto& t : mon.tasks()) {
      tip += QStringLiteral("• %1\n").arg(t.summary());
    }
    tip += QStringLiteral("\nClick for details and log");
    setToolTip(tip.trimmed());
  } else {
    setIcon(theme_icon("emblem-ok", "dialog-information"));
    setToolTip(QStringLiteral("Idle — click for recent log messages"));
  }
}

void ActivityIndicator::open_dialog()
{
  show_activity_dialog(window());
}

} // namespace dirtoo::app

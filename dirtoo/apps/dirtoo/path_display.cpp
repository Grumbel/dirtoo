// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "path_display.hpp"

#include <QStringList>

#include <algorithm>

namespace dirtoo::app {

QString elide_path_for_title(QString path, int max_chars)
{
  path.replace(QLatin1Char('\\'), QLatin1Char('/'));
  if (path.size() <= max_chars || max_chars < 8) {
    return path;
  }

  const bool absolute = path.startsWith(QLatin1Char('/'));
  QStringList parts = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
  if (parts.isEmpty()) {
    return path;
  }

  auto joined = [&]() {
    QString out;
    for (int i = 0; i < parts.size(); ++i) {
      if (i > 0) {
        out += QLatin1Char('/');
      }
      out += parts[i];
    }
    if (absolute && !out.startsWith(QLatin1Char('/'))) {
      out.prepend(QLatin1Char('/'));
    }
    return out;
  };

  int last_nonempty = -1;
  for (int i = parts.size() - 1; i >= 0; --i) {
    if (!parts[i].isEmpty()) {
      last_nonempty = i;
      break;
    }
  }
  if (last_nonempty < 0) {
    return path;
  }

  auto is_collapsed = [](const QString& s) {
    return s.size() >= 2 && s.endsWith(QChar(0x2026)); // …
  };

  while (joined().size() > max_chars) {
    int victim = -1;
    for (int i = 0; i < last_nonempty; ++i) {
      if (parts[i].isEmpty()) {
        continue;
      }
      if (!is_collapsed(parts[i]) && parts[i].size() > 1) {
        victim = i;
        break;
      }
    }
    if (victim < 0) {
      QString j = joined();
      if (j.size() <= max_chars) {
        return j;
      }
      const QString base = parts[last_nonempty];
      const QString suffix = QLatin1Char('/') + base;
      const int keep = max_chars - 1;
      if (keep <= suffix.size()) {
        return QChar(0x2026) + base.right(std::max(1, max_chars - 1));
      }
      return QChar(0x2026) + j.right(max_chars - 1);
    }
    const QChar head = parts[victim].at(0);
    parts[victim] = QString(head) + QChar(0x2026);
  }
  return joined();
}

QString location_display_path(const fs::Location& loc)
{
  if (loc.empty()) {
    return QString();
  }
  if (loc.is_archive() || loc.is_tag() || loc.is_set()) {
    return QString::fromStdString(loc.as_url());
  }
  return QString::fromStdString(loc.as_path().string());
}

WindowTitleTexts make_window_title_texts(const fs::Location& location, bool read_only)
{
  WindowTitleTexts out;
  const QString path = location_display_path(location);
  const QString title_path =
      path.isEmpty() ? QString() : elide_path_for_title(path, /*max_chars=*/96);
  const QString icon_path =
      path.isEmpty() ? QString() : elide_path_for_title(path, /*max_chars=*/20);

  if (!title_path.isEmpty()) {
    out.window_title = title_path + QStringLiteral(" — dirtoo");
  } else {
    out.window_title = QStringLiteral("dirtoo");
  }
  if (read_only) {
    out.window_title += QStringLiteral(" [read-only]");
  }

  if (!icon_path.isEmpty()) {
    out.icon_text = icon_path;
    if (read_only) {
      out.icon_text += QStringLiteral(" [ro]");
    }
  } else {
    out.icon_text = read_only ? QStringLiteral("dirtoo [ro]") : QStringLiteral("dirtoo");
  }
  return out;
}

} // namespace dirtoo::app

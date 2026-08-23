// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <QUrl>

#include <filesystem>
#include <optional>
#include <string>

namespace dirtoo::app {

/// Best-effort Location from a drop URL (plain file or Python-style archive URL).
[[nodiscard]] inline std::optional<fs::Location> location_from_drop_url(const QUrl& url)
{
  // Prefer the string form so "//archive:entry" survives Qt path normalization.
  const QString s = url.toString();
  if (s.contains(QLatin1String("//archive")) || s.startsWith(QLatin1String("archive://"))) {
    try {
      return fs::Location::from_url(s.toStdString());
    } catch (...) {
      // fall through
    }
  }
  const QByteArray enc = url.toEncoded();
  if (!enc.isEmpty()) {
    const std::string es = enc.toStdString();
    if (es.find("//archive") != std::string::npos || es.starts_with("archive://")) {
      try {
        return fs::Location::from_url(es);
      } catch (...) {
      }
    }
  }
  if (url.isLocalFile()) {
    const QString local = url.toLocalFile();
    // toLocalFile may leave "//archive:…" in the path for our custom URLs.
    if (local.contains(QLatin1String("//archive"))) {
      try {
        return fs::Location::from_url(("file://" + local).toStdString());
      } catch (...) {
      }
    }
    return fs::Location::from_path(std::filesystem::path{local.toStdString()});
  }
  return std::nullopt;
}

} // namespace dirtoo::app

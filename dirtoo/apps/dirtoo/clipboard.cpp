// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboard.hpp"

#include <QMimeData>
#include <QUrl>

namespace dirtoo::app {
namespace {

const char* kDirtooMime = "application/x-dirtoo-clipboard";
const char* kGnomeMime = "x-special/gnome-copied-files";

} // namespace

ClipboardPayload parse_dirtoo_clipboard_text(const QString& text)
{
  ClipboardPayload payload;
  const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  if (lines.isEmpty()) {
    return payload;
  }
  if (lines.front() == QLatin1String("cut")) {
    payload.mode = ClipboardMode::Cut;
  } else if (lines.front() == QLatin1String("link")) {
    payload.mode = ClipboardMode::Link;
  } else {
    payload.mode = ClipboardMode::Copy;
  }
  for (int i = 1; i < lines.size(); ++i) {
    payload.paths.emplace_back(lines[i].toStdString());
  }
  return payload;
}

ClipboardPayload parse_gnome_clipboard_text(const QString& text)
{
  ClipboardPayload payload;
  const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  if (lines.isEmpty()) {
    return payload;
  }
  if (lines.front() == QLatin1String("cut")) {
    payload.mode = ClipboardMode::Cut;
  } else if (lines.front() == QLatin1String("link")) {
    payload.mode = ClipboardMode::Link;
  } else {
    payload.mode = ClipboardMode::Copy;
  }
  for (int i = 1; i < lines.size(); ++i) {
    const QUrl url(lines[i]);
    if (url.isLocalFile()) {
      payload.paths.emplace_back(url.toLocalFile().toStdString());
    }
  }
  return payload;
}

namespace {

QString mode_token(ClipboardMode mode)
{
  switch (mode) {
  case ClipboardMode::Cut:
    return QStringLiteral("cut");
  case ClipboardMode::Link:
    return QStringLiteral("link");
  case ClipboardMode::Copy:
  default:
    return QStringLiteral("copy");
  }
}

} // namespace

QMimeData* make_clipboard_mime(ClipboardMode mode, const std::vector<std::filesystem::path>& paths)
{
  auto* mime = new QMimeData;

  QString body;
  body += mode_token(mode);
  body += QLatin1Char('\n');
  QList<QUrl> urls;
  urls.reserve(static_cast<int>(paths.size()));
  for (const auto& p : paths) {
    body += QString::fromStdString(p.string());
    body += QLatin1Char('\n');
    urls.push_back(QUrl::fromLocalFile(QString::fromStdString(p.string())));
  }

  mime->setData(QString::fromLatin1(kDirtooMime), body.toUtf8());
  mime->setUrls(urls);

  QString gnome = mode_token(mode);
  for (const QUrl& url : urls) {
    gnome += QLatin1Char('\n');
    gnome += url.toString(QUrl::FullyEncoded);
  }
  mime->setData(QString::fromLatin1(kGnomeMime), gnome.toUtf8());

  return mime;
}

ClipboardPayload parse_clipboard_mime(const QMimeData* mime)
{
  ClipboardPayload payload;
  if (mime == nullptr) {
    return payload;
  }

  if (mime->hasFormat(QString::fromLatin1(kDirtooMime))) {
    return parse_dirtoo_clipboard_text(QString::fromUtf8(mime->data(QString::fromLatin1(kDirtooMime))));
  }

  if (mime->hasFormat(QString::fromLatin1(kGnomeMime))) {
    return parse_gnome_clipboard_text(QString::fromUtf8(mime->data(QString::fromLatin1(kGnomeMime))));
  }

  if (mime->hasUrls()) {
    payload.mode = ClipboardMode::Copy;
    for (const QUrl& url : mime->urls()) {
      if (url.isLocalFile()) {
        payload.paths.emplace_back(url.toLocalFile().toStdString());
      }
    }
  }
  return payload;
}

bool clipboard_has_paths(const QMimeData* mime)
{
  if (mime == nullptr) {
    return false;
  }
  if (mime->hasFormat(QString::fromLatin1(kDirtooMime))
      || mime->hasFormat(QString::fromLatin1(kGnomeMime))) {
    return true;
  }
  if (mime->hasUrls()) {
    for (const QUrl& url : mime->urls()) {
      if (url.isLocalFile()) {
        return true;
      }
    }
  }
  return false;
}

} // namespace dirtoo::app

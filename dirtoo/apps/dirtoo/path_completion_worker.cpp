// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "path_completion_worker.hpp"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace dirtoo::app {
namespace {

QString expand_user(const QString& text)
{
  if (text == QLatin1Char('~') || text.startsWith(QStringLiteral("~/"))) {
    const QString home = QDir::homePath();
    if (text.size() == 1) {
      return home + QLatin1Char('/');
    }
    return home + text.mid(1);
  }
  return text;
}

QString common_prefix(const QStringList& items)
{
  if (items.isEmpty()) {
    return {};
  }
  QString prefix = items.front();
  for (int i = 1; i < items.size(); ++i) {
    const QString& s = items[i];
    int n = 0;
    const int lim = std::min(prefix.size(), s.size());
    while (n < lim && prefix[n].toLower() == s[n].toLower()) {
      ++n;
    }
    prefix = prefix.left(n);
    if (prefix.isEmpty()) {
      break;
    }
  }
  return prefix;
}

} // namespace

PathCompletionWorker::PathCompletionWorker(QObject* parent)
    : QObject(parent)
{
}

void PathCompletionWorker::cancel()
{
  cancel_.store(true, std::memory_order_relaxed);
}

void PathCompletionWorker::complete(quint64 request_id, const QString& text)
{
  cancel_.store(false, std::memory_order_relaxed);
  active_id_.store(request_id, std::memory_order_relaxed);

  const QString expanded = expand_user(text);
  if (expanded.isEmpty()) {
    emit completions_ready(request_id, text, {});
    return;
  }

  // Split into directory to scan and basename prefix to match.
  QString dirname;
  QString basename;
  if (expanded.endsWith(QLatin1Char('/'))) {
    dirname = expanded;
    basename.clear();
  } else {
    const QFileInfo fi(expanded);
    dirname = fi.path();
    // QFileInfo::path() returns "." for bare names — treat as cwd.
    if (dirname.isEmpty() || dirname == QLatin1String(".")) {
      dirname = QDir::currentPath();
    }
    basename = fi.fileName();
  }

  if (dirname.isEmpty()) {
    dirname = QStringLiteral("/");
  }

  QStringList candidates;
  std::error_code ec;
  const std::filesystem::path dir_path{dirname.toStdString()};
  const std::string prefix = basename.toStdString();

  auto starts_with_ci = [](const std::string& name, const std::string& pfx) {
    if (pfx.empty()) {
      return true;
    }
    if (name.size() < pfx.size()) {
      return false;
    }
    for (std::size_t i = 0; i < pfx.size(); ++i) {
      const auto a = static_cast<unsigned char>(name[i]);
      const auto b = static_cast<unsigned char>(pfx[i]);
      if (std::tolower(a) != std::tolower(b)) {
        return false;
      }
    }
    return true;
  };

  const auto opts = std::filesystem::directory_options::skip_permission_denied;
  for (const auto& entry : std::filesystem::directory_iterator(dir_path, opts, ec)) {
    if (cancel_.load(std::memory_order_relaxed)
        || active_id_.load(std::memory_order_relaxed) != request_id) {
      emit completions_ready(request_id, text, {});
      return;
    }
    if (ec) {
      break;
    }
    std::error_code is_ec;
    if (!entry.is_directory(is_ec)) {
      continue;
    }
    const auto name = entry.path().filename().string();
    if (!starts_with_ci(name, prefix)) {
      continue;
    }
    // Keep trailing slash so choosing a completion continues into the dir.
    QString full = QString::fromStdString(entry.path().string());
    if (!full.endsWith(QLatin1Char('/'))) {
      full += QLatin1Char('/');
    }
    candidates.push_back(full);
  }

  std::sort(candidates.begin(), candidates.end(), [](const QString& a, const QString& b) {
    return a.toLower() < b.toLower();
  });

  QString longest = candidates.isEmpty() ? expanded : common_prefix(candidates);
  // Prefer returning the expanded form for longest when it is a strict extension.
  if (!candidates.isEmpty() && longest.size() < expanded.size()) {
    longest = expanded;
  }

  emit completions_ready(request_id, longest, candidates);
}

} // namespace dirtoo::app

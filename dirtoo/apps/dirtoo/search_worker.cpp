// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "search_worker.hpp"

#include "dirtoo/filter/parser.hpp"
#include "dirtoo/filter/search.hpp"

namespace dirtoo::app {

SearchWorker::SearchWorker(QObject* parent)
    : QObject(parent)
{
}

void SearchWorker::cancel()
{
  cancel_.store(true, std::memory_order_relaxed);
}

void SearchWorker::start(const QString& root_path, const QString& expression, bool show_hidden,
                         int max_depth)
{
  cancel_.store(false, std::memory_order_relaxed);

  const auto parsed = filter::parse_filter(expression.toStdString());
  if (!parsed) {
    emit finished(0, 0,
                  QStringLiteral("parse error at %1: %2")
                      .arg(parsed.error().position)
                      .arg(QString::fromStdString(parsed.error().message)));
    return;
  }

  filter::SearchOptions opt;
  opt.max_depth = max_depth;
  opt.show_hidden = show_hidden;
  opt.should_cancel = [this] { return cancel_.load(std::memory_order_relaxed); };

  quint64 matched_so_far = 0;
  quint64 visited_hint = 0;
  const auto stats = filter::search_directory(
      std::filesystem::path{root_path.toStdString()}, **parsed, opt,
      [this, &matched_so_far, &visited_hint](const filter::FilterItem& item) {
        ++matched_so_far;
        ++visited_hint;
        emit match_found(QString::fromStdString(item.path.string()), item.is_directory,
                         static_cast<quint64>(item.size));
        if (matched_so_far % 32 == 0) {
          emit progress(visited_hint, matched_so_far);
        }
      });

  QString err;
  if (cancel_.load(std::memory_order_relaxed)) {
    err = QStringLiteral("cancelled");
  }
  emit finished(stats.matched, stats.visited, err);
}

} // namespace dirtoo::app

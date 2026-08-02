// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "search_worker.hpp"

#include "dirtoo/filter/parser.hpp"
#include "dirtoo/filter/search.hpp"

#include <filesystem>

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

  const std::string expr = expression.toStdString();
  auto parsed = filter::parse_filter(expr);
  if (!parsed) {
    emit finished(0, 0,
                  QStringLiteral("Invalid filter expression: %1")
                      .arg(QString::fromStdString(parsed.error().message)));
    return;
  }
  const filter::MatchFuncPtr match = *parsed;

  filter::SearchOptions options;
  options.max_depth = max_depth;
  options.show_hidden = show_hidden;
  options.should_cancel = [this] { return cancel_.load(std::memory_order_relaxed); };

  const std::filesystem::path root{root_path.toStdString()};

  quint64 matched_so_far = 0;
  const auto stats = filter::search_directory(
      root, *match, options, [this, &matched_so_far](const filter::FilterItem& item) {
        ++matched_so_far;
        emit match_found(QString::fromStdString(item.path.string()), item.is_directory,
                         static_cast<quint64>(item.size));
        // Status bar only displays the match count; visited is finalized on finished().
        if ((matched_so_far % 16) == 0) {
          emit progress(0, matched_so_far);
        }
      });

  if (cancel_.load(std::memory_order_relaxed)) {
    emit finished(stats.matched, stats.visited, QStringLiteral("cancelled"));
    return;
  }
  emit finished(stats.matched, stats.visited, QString{});
}

} // namespace dirtoo::app

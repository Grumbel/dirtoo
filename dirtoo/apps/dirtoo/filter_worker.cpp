// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filter_worker.hpp"

#include "dirtoo/collection/grouper.hpp"
#include "dirtoo/filter/filter_item.hpp"
#include "dirtoo/filter/parser.hpp"
#include "dirtoo/filter/predicates.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <optional>

namespace dirtoo::app {
namespace {

bool is_hidden_name(const std::string& name)
{
  return !name.empty() && name[0] == '.';
}

filter::FilterItem to_filter_item(const fs::FileInfo& fi)
{
  filter::FilterItem item{
      .name = fi.basename(),
      .size = fi.size(),
      .is_directory = fi.is_directory(),
      .path = fi.path(),
      .mtime_sec = std::nullopt,
  };
  try {
    const auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(fi.mtime());
    item.mtime_sec =
        std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
  } catch (...) {
    // leave mtime_sec as nullopt for synthetic / unset times
  }
  return item;
}

} // namespace

bool filter_expression_needs_content_io(const QString& expression)
{
  // Predicates that may read file contents or run ffprobe/pdfinfo/bsdtar.
  // Must not run on the GUI thread (lookup_media → resolve_media_cached).
  const QString lower = expression.toLower();
  static const char* keys[] = {
      "contains:",
      "containsre:",
      "contains_regex:",
      "cre:",
      "containsfuzzy:",
      "contains_fuzzy:",
      "cfuzzy:",
      "cfuz:",
      "duration:",
      "width:",
      "height:",
      "framerate:",
      "fps:",
      "pages:",
      "filecount:",
      "file_count:",
      "nfiles:",
  };
  for (const char* k : keys) {
    if (lower.contains(QLatin1String(k))) {
      return true;
    }
  }
  return false;
}

FilterWorker::FilterWorker(QObject* parent)
    : QObject(parent)
{
}

void FilterWorker::filter_items(std::vector<fs::FileInfo> items, const QString& expression,
                               bool show_hidden, collection::GroupMode group_mode,
                               quint64 generation)
{
  filter::MatchFuncPtr match;
  bool parse_ok = true;
  const std::string expr = expression.toStdString();
  if (!expr.empty()) {
    auto parsed = filter::parse_filter(expr);
    if (parsed) {
      match = *parsed;
    } else {
      match = filter::make_name_substring(expr, false);
      parse_ok = false;
    }
  }

  std::vector<fs::FileInfo> visible;
  visible.reserve(items.size());
  for (auto& fi : items) {
    if (!show_hidden && is_hidden_name(fi.basename())) {
      continue;
    }
    if (match != nullptr && !match->matches(to_filter_item(fi))) {
      continue;
    }
    visible.push_back(std::move(fi));
  }

  // Reorder for grouping (Session needs a global mtime-gap pass). Labels are
  // recomputed in FileCollection::replace_visible.
  if (group_mode != collection::GroupMode::None) {
    (void)collection::apply_grouping(visible, group_mode);
  }

  emit filtered(generation, std::move(visible), parse_ok);
}

} // namespace dirtoo::app

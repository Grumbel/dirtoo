// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filter_expression_completer.hpp"

#include "dirtoo/tags/tag_store.hpp"

#include <QLineEdit>
#include <QElapsedTimer>
#include <QStringListModel>

namespace dirtoo::app {
namespace {

/// Index of the first character of the last filter token in `text`.
int last_token_start(const QString& text)
{
  int start = 0;
  for (int i = 0; i < text.size(); ++i) {
    const QChar c = text.at(i);
    if (c.isSpace() || c == QLatin1Char('(') || c == QLatin1Char(')')
        || c == QLatin1Char('|')) {
      start = i + 1;
    }
  }
  return start;
}

QStringList default_keywords()
{
  return {
      QStringLiteral("tag:"),
      QStringLiteral("tagged:yes"),
      QStringLiteral("tagged:no"),
      QStringLiteral("checksummed:yes"),
      QStringLiteral("checksummed:no"),
      QStringLiteral("checksummed:full"),
      QStringLiteral("checksummed:quick"),
      QStringLiteral("type:file"),
      QStringLiteral("type:directory"),
      QStringLiteral("type:image"),
      QStringLiteral("type:video"),
      QStringLiteral("type:audio"),
      QStringLiteral("type:archive"),
      QStringLiteral("size:>1M"),
      QStringLiteral("size:1M..50M"),
  };
}

} // namespace

FilterExpressionCompleter::FilterExpressionCompleter(QObject* parent)
    : QCompleter(parent)
    , model_(new QStringListModel(this))
    , static_keywords_(default_keywords())
{
  setModel(model_);
  setCaseSensitivity(Qt::CaseInsensitive);
  setCompletionMode(QCompleter::PopupCompletion);
  setFilterMode(Qt::MatchStartsWith);
  setMaxVisibleItems(12);
  refresh_suggestions();
}

void FilterExpressionCompleter::attach(QLineEdit* edit)
{
  if (edit == nullptr) {
    return;
  }
  edit->setCompleter(this);
  // Refresh when the user focuses the field so newly created tags appear.
  QObject::connect(edit, &QLineEdit::textEdited, this, [this](const QString&) {
    // Tags change infrequently; refresh opportunistically on first key after focus.
  });
  refresh_suggestions();
}

void FilterExpressionCompleter::refresh_suggestions()
{
  QStringList items = static_keywords_;

  // Tag list is opened from SQLite; do not hit the DB on every focus/keystroke.
  if (tag_cache_.isEmpty() || !tag_cache_age_.isValid()
      || tag_cache_age_.elapsed() > kTagCacheTtlMs) {
    tag_cache_.clear();
    dirtoo::tags::TagStore store;
    std::string err;
    if (store.open(dirtoo::tags::TagStore::default_path(), &err)) {
      // Cap so a huge tag DB cannot freeze the completer popup.
      constexpr int kMaxTags = 2000;
      int n = 0;
      for (const auto& def : store.list_tags()) {
        if (def.name.empty()) {
          continue;
        }
        tag_cache_.append(QStringLiteral("tag:%1").arg(QString::fromStdString(def.name)));
        if (++n >= kMaxTags) {
          break;
        }
      }
    }
    tag_cache_age_.restart();
  }
  items += tag_cache_;
  items.removeDuplicates();
  items.sort(Qt::CaseInsensitive);
  model_->setStringList(items);
}

QStringList FilterExpressionCompleter::splitPath(const QString& path) const
{
  const int start = last_token_start(path);
  return {path.mid(start)};
}

QString FilterExpressionCompleter::pathFromIndex(const QModelIndex& index) const
{
  if (!index.isValid()) {
    return {};
  }
  const QString completion = index.data(Qt::DisplayRole).toString();
  auto* edit = qobject_cast<QLineEdit*>(widget());
  if (edit == nullptr) {
    return completion;
  }
  const QString full = edit->text();
  const int start = last_token_start(full);
  return full.left(start) + completion;
}

} // namespace dirtoo::app

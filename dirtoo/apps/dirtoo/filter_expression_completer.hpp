// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QCompleter>
#include <QString>
#include <QStringList>
#include <QElapsedTimer>

class QLineEdit;
class QStringListModel;

namespace dirtoo::app {

/// Token-aware completer for filter / recursive-search expressions.
/// Completes the last whitespace- or operator-delimited token so multi-term
/// expressions keep earlier tokens. Suggests known tags as `tag:name` and
/// fixed keywords (`tagged:yes|no`, `checksummed:…`, common type:… keys).
class FilterExpressionCompleter : public QCompleter {
  Q_OBJECT
public:
  explicit FilterExpressionCompleter(QObject* parent = nullptr);

  /// Attach to a line edit (sets completer + refreshes tag list).
  void attach(QLineEdit* edit);

  /// Reload tag: suggestions from TagStore (safe to call often; cheap if DB closed).
  void refresh_suggestions();

protected:
  /// Last token only (after space, `(`, `)`, `|`).
  [[nodiscard]] QStringList splitPath(const QString& path) const override;
  /// Replace last token in the line edit with the chosen completion.
  [[nodiscard]] QString pathFromIndex(const QModelIndex& index) const override;

private:
  QStringListModel* model_ = nullptr;
  QStringList static_keywords_;
  QStringList tag_cache_;
  QElapsedTimer tag_cache_age_;
  static constexpr qint64 kTagCacheTtlMs = 5000;
};

} // namespace dirtoo::app

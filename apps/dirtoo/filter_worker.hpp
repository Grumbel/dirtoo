// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/collection/grouper.hpp"
#include "dirtoo/fs/file_info.hpp"

#include <QObject>
#include <QString>

#include <vector>

namespace dirtoo::app {

/// Applies a filter expression (including content predicates) off the GUI thread.
class FilterWorker : public QObject {
  Q_OBJECT

public:
  explicit FilterWorker(QObject* parent = nullptr);

public slots:
  void filter_items(std::vector<dirtoo::fs::FileInfo> items, const QString& expression,
                    bool show_hidden, dirtoo::collection::GroupMode group_mode,
                    quint64 generation);

signals:
  void filtered(quint64 generation, std::vector<dirtoo::fs::FileInfo> visible, bool parse_ok);
};

/// True when the expression may read file contents (must not run on the GUI thread).
[[nodiscard]] bool filter_expression_needs_content_io(const QString& expression);

} // namespace dirtoo::app

Q_DECLARE_METATYPE(dirtoo::collection::GroupMode)

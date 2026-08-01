// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/collection/sorter.hpp"
#include "dirtoo/fs/file_info.hpp"

#include <QObject>

#include <vector>

namespace dirtoo::app {

/// Sorts a FileInfo vector off the GUI thread using dirtoo::collection::Sorter settings.
class SortWorker : public QObject {
  Q_OBJECT

public:
  explicit SortWorker(QObject* parent = nullptr);

public slots:
  void sort_items(std::vector<dirtoo::fs::FileInfo> items, dirtoo::collection::SortKey key,
                  bool ascending, bool directories_first, quint64 generation);

signals:
  void sorted(quint64 generation, std::vector<dirtoo::fs::FileInfo> items);
};

} // namespace dirtoo::app

Q_DECLARE_METATYPE(dirtoo::collection::SortKey)

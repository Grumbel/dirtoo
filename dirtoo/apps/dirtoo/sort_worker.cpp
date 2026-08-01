// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sort_worker.hpp"

namespace dirtoo::app {

SortWorker::SortWorker(QObject* parent)
    : QObject(parent)
{
}

void SortWorker::sort_items(std::vector<fs::FileInfo> items, collection::SortKey key, bool ascending,
                            bool directories_first, quint64 generation)
{
  collection::Sorter sorter;
  sorter.set_key(key);
  sorter.set_ascending(ascending);
  sorter.set_directories_first(directories_first);
  sorter.sort(items);
  emit sorted(generation, std::move(items));
}

} // namespace dirtoo::app

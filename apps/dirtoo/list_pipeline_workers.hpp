// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "directory_load_worker.hpp"
#include "filter_worker.hpp"
#include "sort_worker.hpp"

#include <QObject>
#include <QThread>

#include <cstdint>

namespace dirtoo::app {

/// Owns directory-load, sort, and filter worker threads for the file list pipeline.
class ListPipelineWorkers : public QObject {
  Q_OBJECT
public:
  explicit ListPipelineWorkers(QObject* parent = nullptr);
  ~ListPipelineWorkers() override;

  void setup();
  void shutdown();

  [[nodiscard]] DirectoryLoadWorker* dir_load() const { return dir_load_worker_; }
  [[nodiscard]] SortWorker* sort() const { return sort_worker_; }
  [[nodiscard]] FilterWorker* filter() const { return filter_worker_; }

  [[nodiscard]] quint64& dir_load_generation() { return dir_load_generation_; }
  [[nodiscard]] quint64 dir_load_generation() const { return dir_load_generation_; }
  [[nodiscard]] quint64& sort_generation() { return sort_generation_; }
  [[nodiscard]] quint64 sort_generation() const { return sort_generation_; }
  [[nodiscard]] quint64& filter_generation() { return filter_generation_; }
  [[nodiscard]] quint64 filter_generation() const { return filter_generation_; }

  /// Next generation ids (post-increment).
  quint64 next_dir_load_generation() { return ++dir_load_generation_; }
  quint64 next_sort_generation() { return ++sort_generation_; }
  quint64 next_filter_generation() { return ++filter_generation_; }

private:
  QThread* dir_load_thread_ = nullptr;
  DirectoryLoadWorker* dir_load_worker_ = nullptr;
  quint64 dir_load_generation_ = 0;

  QThread* sort_thread_ = nullptr;
  SortWorker* sort_worker_ = nullptr;
  quint64 sort_generation_ = 0;

  QThread* filter_thread_ = nullptr;
  FilterWorker* filter_worker_ = nullptr;
  quint64 filter_generation_ = 0;
};

} // namespace dirtoo::app

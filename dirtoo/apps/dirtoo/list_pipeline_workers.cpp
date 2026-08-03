// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "list_pipeline_workers.hpp"

#include "dirtoo/collection/file_collection.hpp"
#include "dirtoo/fs/file_info.hpp"

#include <QMetaType>

#include <vector>

namespace dirtoo::app {

ListPipelineWorkers::ListPipelineWorkers(QObject* parent)
    : QObject(parent)
{
}

ListPipelineWorkers::~ListPipelineWorkers()
{
  shutdown();
}

void ListPipelineWorkers::setup()
{
  if (dir_load_worker_ != nullptr) {
    return;
  }

  dir_load_worker_ = new DirectoryLoadWorker;
  dir_load_thread_ = new QThread(this);
  dir_load_worker_->moveToThread(dir_load_thread_);
  connect(dir_load_thread_, &QThread::finished, dir_load_worker_, &QObject::deleteLater);
  qRegisterMetaType<std::vector<dirtoo::fs::FileInfo>>("std::vector<dirtoo::fs::FileInfo>");
  dir_load_thread_->start();

  sort_worker_ = new SortWorker;
  sort_thread_ = new QThread(this);
  sort_worker_->moveToThread(sort_thread_);
  connect(sort_thread_, &QThread::finished, sort_worker_, &QObject::deleteLater);
  qRegisterMetaType<dirtoo::collection::SortKey>("dirtoo::collection::SortKey");
  sort_thread_->start();

  filter_worker_ = new FilterWorker;
  filter_thread_ = new QThread(this);
  filter_worker_->moveToThread(filter_thread_);
  connect(filter_thread_, &QThread::finished, filter_worker_, &QObject::deleteLater);
  qRegisterMetaType<dirtoo::collection::GroupMode>("dirtoo::collection::GroupMode");
  filter_thread_->start();
}

void ListPipelineWorkers::shutdown()
{
  auto stop = [](QThread*& th) {
    if (th != nullptr) {
      th->quit();
      th->wait(3000);
      th = nullptr;
    }
  };
  stop(dir_load_thread_);
  dir_load_worker_ = nullptr;
  stop(sort_thread_);
  sort_worker_ = nullptr;
  stop(filter_thread_);
  filter_worker_ = nullptr;
}

} // namespace dirtoo::app

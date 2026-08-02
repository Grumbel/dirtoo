// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "search_controller.hpp"

#include "search_worker.hpp"

#include <QThread>

namespace dirtoo::app {

SearchController::SearchController(QObject* parent)
    : QObject(parent)
{
}

SearchController::~SearchController()
{
  stop();
}

void SearchController::cleanup_thread()
{
  if (thread_ == nullptr) {
    return;
  }
  thread_->quit();
  thread_->wait(3000);
  thread_->deleteLater();
  thread_ = nullptr;
  worker_ = nullptr;
  running_ = false;
}

void SearchController::stop()
{
  if (worker_ != nullptr) {
    worker_->cancel();
  }
  cleanup_thread();
}

void SearchController::start(const QString& root_path, const QString& expression, bool show_hidden,
                             int max_depth)
{
  stop();

  thread_ = new QThread(this);
  worker_ = new SearchWorker();
  worker_->moveToThread(thread_);

  connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
  connect(worker_, &SearchWorker::match_found, this, &SearchController::match_found);
  connect(worker_, &SearchWorker::progress, this, &SearchController::progress);
  connect(worker_, &SearchWorker::finished, this, [this](quint64 matched, quint64 visited,
                                                         const QString& error) {
    running_ = false;
    emit finished(matched, visited, error);
    cleanup_thread();
  });

  connect(thread_, &QThread::started, worker_, [worker = worker_, root_path, expression, show_hidden,
                                                max_depth] {
    worker->start(root_path, expression, show_hidden, max_depth);
  });

  running_ = true;
  thread_->start();
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "path_completion_service.hpp"

#include <QMetaObject>

namespace dirtoo::app {

PathCompletionService::PathCompletionService(QObject* parent)
    : QObject(parent)
{
}

PathCompletionService::~PathCompletionService()
{
  shutdown();
}

void PathCompletionService::setup(QLineEdit* edit)
{
  edit_ = edit;
  if (edit_ == nullptr) {
    return;
  }
  model_ = new QStringListModel(this);
  completer_ = new QCompleter(model_, edit_);
  completer_->setCaseSensitivity(Qt::CaseInsensitive);
  completer_->setCompletionMode(QCompleter::PopupCompletion);
  completer_->setFilterMode(Qt::MatchStartsWith);
  edit_->setCompleter(completer_);

  timer_ = new QTimer(this);
  timer_->setSingleShot(true);
  timer_->setInterval(60);
  connect(timer_, &QTimer::timeout, this, &PathCompletionService::on_timeout);

  thread_ = new QThread(this);
  worker_ = new PathCompletionWorker();
  worker_->moveToThread(thread_);
  connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
  connect(worker_, &PathCompletionWorker::completions_ready, this,
          &PathCompletionService::on_completions_ready);
  // Re-emit for MainWindow if it still wants a slot (optional)
  connect(worker_, &PathCompletionWorker::completions_ready, this,
          &PathCompletionService::completions_ready);
  thread_->start();
}

void PathCompletionService::shutdown()
{
  if (worker_ != nullptr) {
    worker_->cancel();
  }
  if (thread_ != nullptr) {
    thread_->quit();
    thread_->wait(2000);
    thread_ = nullptr;
    worker_ = nullptr;
  }
}

void PathCompletionService::on_text_edited(const QString& text)
{
  pending_ = text;
  if (timer_ != nullptr) {
    timer_->start();
  }
}

void PathCompletionService::on_timeout()
{
  if (worker_ == nullptr || thread_ == nullptr) {
    return;
  }
  const QString text = pending_;
  if (text.isEmpty()) {
    if (model_ != nullptr) {
      model_->setStringList({});
    }
    return;
  }
  const quint64 id = ++request_id_;
  QMetaObject::invokeMethod(
      worker_, [worker = worker_, id, text] { worker->complete(id, text); },
      Qt::QueuedConnection);
}

void PathCompletionService::on_completions_ready(quint64 request_id, const QString& longest,
                                                 const QStringList& candidates)
{
  (void)longest;
  if (request_id != request_id_) {
    return;
  }
  if (model_ == nullptr) {
    return;
  }
  model_->setStringList(candidates);
  if (completer_ != nullptr && edit_ != nullptr && edit_->hasFocus() && !candidates.isEmpty()) {
    completer_->complete();
  }
}

} // namespace dirtoo::app

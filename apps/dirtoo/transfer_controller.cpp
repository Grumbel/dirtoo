// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "transfer_controller.hpp"

#include "transfer_dialog.hpp"

#include <QMetaObject>

namespace dirtoo::app {

TransferController::TransferController(QObject* parent)
    : QObject(parent)
{
  worker_ = new TransferWorker;
  worker_->moveToThread(&thread_);
  connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);

  connect(worker_, &TransferWorker::item_started, this, &TransferController::item_started);
  connect(worker_, &TransferWorker::byte_progress, this, &TransferController::byte_progress);
  connect(worker_, &TransferWorker::conflict_required, this, &TransferController::conflict_required);
  connect(worker_, &TransferWorker::finished, this, [this](TransferSummary summary) {
    busy_ = false;
    emit finished(summary);
  });
  connect(worker_, &TransferWorker::log_line, this, &TransferController::log_line);

  thread_.start();
}

TransferController::~TransferController()
{
  shutdown();
}

void TransferController::shutdown()
{
  if (worker_ != nullptr) {
    worker_->cancel();
  }
  thread_.quit();
  thread_.wait(5000);
  worker_ = nullptr;
  dialog_ = nullptr;
  busy_ = false;
}

void TransferController::start(QWidget* dialog_parent, const TransferRequest& request)
{
  if (worker_ == nullptr || busy_) {
    return;
  }
  last_mode_ = request.mode;
  busy_ = true;

  if (dialog_ == nullptr) {
    dialog_ = new TransferDialog(dialog_parent);
    const auto cancel_worker = [this] {
      if (worker_ != nullptr) {
        worker_->cancel();
      }
    };
    connect(dialog_, &TransferDialog::cancel_requested, this, cancel_worker);
    connect(dialog_, &QDialog::rejected, this, cancel_worker);
    connect(dialog_, &TransferDialog::pause_requested, this, [this] {
      if (worker_ != nullptr) {
        worker_->pause();
      }
    });
    connect(dialog_, &TransferDialog::resume_requested, this, [this] {
      if (worker_ != nullptr) {
        worker_->resume();
      }
    });
  }

  dialog_->reset();
  dialog_->set_title_text(request.mode == ClipboardMode::Cut ? QStringLiteral("Moving…")
                                                             : QStringLiteral("Copying…"));
  dialog_->set_destination(
      QString::fromStdString(request.destination_directory.string()));
  dialog_->show();
  dialog_->raise();

  QMetaObject::invokeMethod(worker_, [this, request] { worker_->run(request); },
                            Qt::QueuedConnection);
}

void TransferController::resolve_conflict(dirops::ConflictPolicy policy, bool apply,
                                          bool apply_to_all)
{
  if (worker_ != nullptr) {
    worker_->resolve_conflict(policy, apply, apply_to_all);
  }
}

} // namespace dirtoo::app

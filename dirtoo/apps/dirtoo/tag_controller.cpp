// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tag_controller.hpp"

#include "tag_job.hpp"
#include "activity_monitor.hpp"

#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>

namespace dirtoo::app {

TagController::TagController(QObject* parent)
    : QObject(parent)
{
}

void TagController::set_dialog_parent(QWidget* parent)
{
  dialog_parent_ = parent;
}

void TagController::tag_files(std::vector<dirtoo::fs::FileInfo> selection)
{
  std::vector<dirtoo::fs::FileInfo> files;
  files.reserve(selection.size());
  for (auto& fi : selection) {
    if (!fi.is_regular_file()) {
      continue;
    }
    if (fi.is_synthetic() && !fi.location().is_archive()) {
      continue;
    }
    files.push_back(std::move(fi));
  }
  if (files.empty()) {
    QMessageBox::information(dialog_parent_, QStringLiteral("Tag"),
                             QStringLiteral("Select one or more regular files first "
                                            "(disk files or archive members)."));
    return;
  }

  bool ok = false;
  const QString tag = QInputDialog::getText(
      dialog_parent_, QStringLiteral("Tag"),
      QStringLiteral("Tag name for %1 file(s):").arg(static_cast<int>(files.size())),
      QLineEdit::Normal, QString(), &ok);
  if (!ok || tag.trimmed().isEmpty()) {
    return;
  }

  const int total = static_cast<int>(files.size());
  auto* progress = new QProgressDialog(QStringLiteral("Tagging files…"), QStringLiteral("Cancel"),
                                       0, total, dialog_parent_);
  progress->setWindowModality(Qt::WindowModal);
  progress->setMinimumDuration(total > 1 ? 0 : 2000);
  progress->setAttribute(Qt::WA_DeleteOnClose);
  progress->setValue(0);

  // Hash + archive extract on a worker thread (AGENTS: no hash on GUI).
  auto* job = new TagJob(std::move(files), tag.trimmed(), this);
  connect(progress, &QProgressDialog::canceled, job, &TagJob::cancel);
  connect(job, &TagJob::progress, this, [progress](int done, int total_n, const QString& name) {
    if (progress == nullptr) {
      return;
    }
    progress->setMaximum(total_n);
    progress->setValue(done);
    if (!name.isEmpty()) {
      progress->setLabelText(QStringLiteral("Tagging %1…").arg(name));
    }
    ActivityMonitor::instance().set_task(QStringLiteral("tag"), QStringLiteral("Tagging"), done,
                                         total_n);
  });
  connect(job, &TagJob::failed, this, [this, progress, job](const QString& message) {
    if (progress != nullptr) {
      progress->close();
    }
    ActivityMonitor::instance().clear_task(QStringLiteral("tag"));
    QMessageBox::warning(dialog_parent_, QStringLiteral("Tag"), message);
    job->deleteLater();
  });
  connect(job, &TagJob::finished, this,
          [this, progress, job](int tagged, int skipped, const QStringList& problems) {
            if (progress != nullptr) {
              progress->setValue(progress->maximum());
              progress->close();
            }
            QString msg = QStringLiteral("Tagged %1 file(s).").arg(tagged);
            if (skipped > 0) {
              msg += QStringLiteral(" Skipped %1.").arg(skipped);
            }
            if (!problems.isEmpty() && problems.size() <= 5) {
              msg += QLatin1Char('\n') + problems.join(QLatin1Char('\n'));
            }
            ActivityMonitor::instance().clear_task(QStringLiteral("tag"));
            emit status_message(msg, 5000);
            if (tagged > 0) {
              emit tags_applied(tagged);
            }
            if (skipped > 0) {
              QMessageBox::warning(dialog_parent_, QStringLiteral("Tag"), msg);
            }
            job->deleteLater();
          });
  job->start();
  progress->show();
}

} // namespace dirtoo::app

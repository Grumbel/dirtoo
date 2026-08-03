#include "main_window.hpp"
#include "archive_member_cache.hpp"
#include "clipboard.hpp"
#include "conflict_dialog.hpp"
#include "operations_history.hpp"
#include "location_url.hpp"

#include "dirtoo/archive/archive_index.hpp"
#include "dirops/ops.hpp"

#include <QApplication>
#include <QDateTime>
#include <QClipboard>
#include <QMessageBox>
#include <QUrl>
#include <QtConcurrent>
#include <QMetaObject>
#include <QStandardPaths>

#include <filesystem>

namespace dirtoo::app {

void MainWindow::start_transfer(const TransferRequest& request)
{
  if (!ensure_mutations_allowed()) {
    return;
  }
  if (transfer_controller_.busy()) {
    set_status(QStringLiteral("A transfer is already in progress"));
    return;
  }
  qInfo().noquote() << QStringLiteral("%1 %2 item(s) → %3")
                           .arg(request.mode == ClipboardMode::Cut ? QStringLiteral("move")
                                                                  : QStringLiteral("copy"))
                           .arg(request.sources.size())
                           .arg(QString::fromStdString(request.destination_directory.string()));
  for (const auto& src : request.sources) {
    qDebug().noquote() << QStringLiteral("  source: %1").arg(QString::fromStdString(src.string()));
  }
  update_edit_actions();
  transfer_controller_.start(this, request);
}

void MainWindow::on_transfer_item_started(int index, int total, const QString& path)
{
  if (transfer_controller_.dialog() != nullptr) {
    transfer_controller_.dialog()->set_item_progress(index, total);
    transfer_controller_.dialog()->set_current_file(path);
  }
}

void MainWindow::on_transfer_byte_progress(quint64 done, quint64 total, const QString& path)
{
  if (transfer_controller_.dialog() != nullptr) {
    transfer_controller_.dialog()->set_current_file(path);
    transfer_controller_.dialog()->set_progress(done, total);
  }
}

void MainWindow::on_transfer_conflict(const QString& destination_name, const QString& source_path,
                                      const QString& destination_path)
{
  // Runs on UI thread (QueuedConnection from worker signal).
  qInfo().noquote() << QStringLiteral("transfer conflict: %1 (src=%2 dest=%3)")
                           .arg(destination_name, source_path, destination_path);
  // resolve_conflict / cancel MUST be invoked directly: the worker thread is blocked
  // waiting on conflict_cv_, so a QueuedConnection to the worker would never run (deadlock).
  if (transfer_controller_.worker() == nullptr) {
    return;
  }
  const auto chosen = ask_conflict_policy(
      this, destination_name, std::filesystem::path{source_path.toStdString()},
      std::filesystem::path{destination_path.toStdString()});
  if (!chosen) {
    transfer_controller_.resolve_conflict(dirops::ConflictPolicy::Fail, false, false);
  } else {
    const auto decision = *chosen;
    transfer_controller_.resolve_conflict(decision.policy, true, decision.apply_to_all);
  }
}

void MainWindow::on_transfer_finished(TransferSummary summary)
{
  qInfo().noquote() << QStringLiteral("transfer finished: done=%1 skipped=%2 cancelled=%3 error=%4")
                           .arg(summary.completed)
                           .arg(summary.skipped)
                           .arg(summary.cancelled)
                           .arg(summary.error.isEmpty() ? QStringLiteral("-") : summary.error);

  /* busy cleared by TransferController */

  if (transfer_controller_.dialog() != nullptr) {
    transfer_controller_.dialog()->mark_finished(summary.cancelled, summary.error);
  } else if (!summary.error.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("Transfer"), summary.error);
  }

  if (transfer_controller_.last_mode() == ClipboardMode::Cut && summary.completed > 0 && !summary.cancelled) {
    QApplication::clipboard()->clear();
  }

  if (summary.cancelled) {
    set_status(QStringLiteral("Transfer cancelled (%1 done, %2 skipped)")
                               .arg(summary.completed)
                               .arg(summary.skipped));
  } else if (!summary.error.isEmpty()) {
    set_status(summary.error);
  } else {
    set_status(QStringLiteral("Transfer: %1 done, %2 skipped")
                               .arg(summary.completed)
                               .arg(summary.skipped));
  }

  {
    OperationHistoryEntry e;
    e.when = QDateTime::currentDateTime();
    e.kind = summary.mode == ClipboardMode::Cut ? OperationKind::Move : OperationKind::Copy;
    e.outcome = summary.cancelled ? QStringLiteral("cancelled")
                : (!summary.error.isEmpty() ? QStringLiteral("failed")
                   : (summary.skipped > 0 && summary.completed > 0 ? QStringLiteral("partial")
                                                                   : QStringLiteral("success")));
    e.detail = summary.error.isEmpty()
                   ? QStringLiteral("%1 done, %2 skipped").arg(summary.completed).arg(summary.skipped)
                   : summary.error;
    e.completed = summary.completed;
    e.skipped = summary.skipped;
    e.destination = QString::fromStdString(summary.destination_directory.string());
    if (e.destination.isEmpty()) {
      e.destination = QString::fromStdString(location_.as_path().string());
    }
    for (const auto& src : summary.sources) {
      e.sources << QString::fromStdString(src.string());
    }
    for (const auto& it : summary.items) {
      OperationItem oi;
      oi.source = QString::fromStdString(it.source.string());
      oi.destination = QString::fromStdString(it.destination.string());
      oi.skipped = it.skipped;
      e.items.push_back(std::move(oi));
      if (!oi.destination.isEmpty()) {
        e.destinations << oi.destination;
      }
    }
    operations_history().record(std::move(e));
  }

  on_directory_changed();
  update_edit_actions();
}

void MainWindow::begin_transfer_from_urls(const QList<QUrl>& urls, Qt::DropAction action,
                                         const std::filesystem::path& dest_dir)
{
  if (transfer_controller_.busy() || urls.isEmpty()) {
    return;
  }

  // Collect plain paths and archive members that still need materialization.
  std::vector<std::filesystem::path> ready;
  struct PendingMember {
    std::filesystem::path archive_file;
    std::filesystem::path member;
  };
  std::vector<PendingMember> pending;
  bool any_from_archive = false;

  for (const QUrl& url : urls) {
    const auto loc = location_from_drop_url(url);
    if (!loc) {
      continue;
    }
    if (loc->is_archive()) {
      any_from_archive = true;
      if (loc->entry_path().empty()) {
        // Drag of the archive root → the archive file itself.
        ready.push_back(loc->as_path());
        continue;
      }
      const fs::Location archive_root = fs::Location::from_archive(loc->as_path(), {});
      if (const auto root = archive_manager_.extracted_root(archive_root)) {
        const auto real = *root / loc->entry_path();
        std::error_code ec;
        if (std::filesystem::exists(real, ec) && !ec) {
          ready.push_back(real);
          continue;
        }
      }
      pending.push_back(PendingMember{loc->as_path(), loc->entry_path()});
      continue;
    }
    ready.push_back(loc->as_path());
  }

  if (ready.empty() && pending.empty()) {
    set_status(QStringLiteral("Drop ignored (no usable paths)"));
    return;
  }

  // Archive members are read-only sources: never Move/Link out of the archive.
  const Qt::DropAction effective =
      any_from_archive && action != Qt::CopyAction ? Qt::CopyAction : action;

  auto finish = [this, dest_dir, effective](std::vector<std::filesystem::path> sources) {
    // Refuse dropping a selection into itself / a selected folder.
    {
      const auto dest_path = dest_dir.lexically_normal();
      for (const auto& src : sources) {
        std::error_code ec;
        if (std::filesystem::equivalent(src, dest_path, ec)) {
          set_status(QStringLiteral("Cannot drop an item onto itself"));
          return;
        }
        if (std::filesystem::is_directory(src, ec)) {
          const auto rel = dest_path.lexically_relative(src.lexically_normal());
          if (!rel.empty() && *rel.begin() != "..") {
            set_status(QStringLiteral("Cannot drop into a selected folder"));
            return;
          }
        }
      }
    }

    if (effective == Qt::LinkAction) {
      int ok = 0;
      int fail = 0;
      for (const auto& src : sources) {
        const auto link = dest_dir / src.filename();
        auto result = dirops::create_symlink(src, link);
        if (result) {
          ++ok;
          operations_history().record_simple(OperationKind::Symlink, {src}, link, true);
        } else {
          ++fail;
          operations_history().record_simple(OperationKind::Symlink, {src}, link, false,
                                             QString::fromStdString(result.error().to_string()));
        }
      }
      set_status(QStringLiteral("Linked %1 (%2 failed)").arg(ok).arg(fail));
      on_directory_changed();
      return;
    }

    TransferRequest req;
    req.mode = (effective == Qt::MoveAction) ? ClipboardMode::Cut : ClipboardMode::Copy;
    req.destination_directory = dest_dir;
    for (const auto& src : sources) {
      const auto target = req.destination_directory / src.filename();
      if (src == req.destination_directory || src == target) {
        continue;
      }
      req.sources.push_back(src);
    }
    if (req.sources.empty()) {
      set_status(QStringLiteral("Drop ignored (invalid targets)"));
      return;
    }
    start_transfer(req);
  };

  if (pending.empty()) {
    finish(std::move(ready));
    return;
  }

  set_status(QStringLiteral("Extracting %1 archive member(s)…").arg(pending.size()));
  const auto cache_root = archive_member_cache_root("dirtoo-archive-drop");
  (void)QtConcurrent::run([this, pending, ready, cache_root, finish]() mutable {
    for (const auto& p : pending) {
      const auto dest = archive_member_dest_dir(cache_root, p.archive_file);
      if (auto extracted =
              ensure_archive_member_extracted(p.archive_file, p.member, dest)) {
        ready.push_back(*extracted);
      }
    }
    QMetaObject::invokeMethod(
        this,
        [this, ready = std::move(ready), finish]() mutable {
          if (ready.empty()) {
            set_status(QStringLiteral("Failed to extract archive member(s) for drop"));
            return;
          }
          finish(std::move(ready));
        },
        Qt::QueuedConnection);
  });
}

void MainWindow::on_urls_dropped_to(const QList<QUrl>& urls, Qt::DropAction action,
                                   const QString& dest_dir)
{
  qInfo().noquote() << QStringLiteral("drop: %1 url(s) action=%2 dest=%3")
                           .arg(urls.size())
                           .arg(int(action))
                           .arg(dest_dir.isEmpty() ? QStringLiteral("(cwd)") : dest_dir);
  if (!ensure_mutations_allowed()) {
    return;
  }
  if (transfer_controller_.busy() || urls.isEmpty()) {
    return;
  }

  // Dropping into the current view while browsing an archive is read-only.
  if (dest_dir.isEmpty() && location_.is_archive()) {
    set_status(QStringLiteral("Cannot drop into an archive (read-only)"));
    return;
  }

  const auto dest = !dest_dir.isEmpty()
                        ? std::filesystem::path{dest_dir.toStdString()}
                        : location_.as_path();
  begin_transfer_from_urls(urls, action, dest);
}

void MainWindow::on_breadcrumb_drop(const fs::Location& target, const QList<QUrl>& urls,
                                   Qt::DropAction action)
{
  if (target.is_archive()) {
    set_status(QStringLiteral("Cannot drop into an archive (read-only)"));
    return;
  }
  if (transfer_controller_.busy() || urls.isEmpty()) {
    return;
  }
  begin_transfer_from_urls(urls, action, target.as_path());
}



} // namespace dirtoo::app

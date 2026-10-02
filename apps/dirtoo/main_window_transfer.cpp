// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include "async_io.hpp"

#include "archive_member_cache.hpp"
#include "clipboard.hpp"
#include "conflict_dialog.hpp"
#include "operations_history.hpp"
#include "location_url.hpp"
#include "dirtoo/archive/archive_index.hpp"
#include "dirops/ops.hpp"
#include <QDateTime>
#include <QUrl>
#include <QtConcurrent>
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
  update_busy_indicator(QStringLiteral("Transferring files…"));
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
                                      const QString& destination_path, ConflictProbe probe)
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
      std::filesystem::path{destination_path.toStdString()}, probe);
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
  update_busy_indicator(status_label_ != nullptr ? status_label_->text() : QString());

  if (transfer_controller_.dialog() != nullptr) {
    transfer_controller_.dialog()->mark_finished(summary.cancelled, summary.error);
  } else if (!summary.error.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("Transfer"), summary.error);
    qWarning().noquote() << QStringLiteral("transfer error: %1").arg(summary.error);
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
      if (!oi.destination.isEmpty()) {
        e.destinations << oi.destination;
      }
      e.items.push_back(std::move(oi));
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

  // Collect plain paths and archive members (pure lookups — no I/O here).
  struct MemberSource {
    std::filesystem::path archive_file;
    std::filesystem::path member;
    std::filesystem::path extracted_candidate; ///< from an already extracted archive, if any
  };
  std::vector<std::filesystem::path> plain;
  std::vector<MemberSource> members;
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
        plain.push_back(loc->as_path());
        continue;
      }
      MemberSource m{loc->as_path(), loc->entry_path(), {}};
      const fs::Location archive_root = fs::Location::from_archive(loc->as_path(), {});
      if (const auto root = archive_manager_.extracted_root(archive_root)) {
        m.extracted_candidate = *root / loc->entry_path();
      }
      members.push_back(std::move(m));
      continue;
    }
    plain.push_back(loc->as_path());
  }

  if (plain.empty() && members.empty()) {
    set_status(QStringLiteral("Drop ignored (no usable paths)"));
    return;
  }

  // Archive members are read-only sources: never Move/Link out of the archive.
  const Qt::DropAction effective =
      any_from_archive && action != Qt::CopyAction ? Qt::CopyAction : action;
  const bool link = effective == Qt::LinkAction;

  struct LinkOutcome {
    std::filesystem::path source;
    std::filesystem::path link;
    QString error; ///< empty on success
  };
  struct DropPlan {
    std::vector<std::filesystem::path> sources;
    QString refusal; ///< non-empty: abort with this status message
    bool linked = false;
    std::vector<LinkOutcome> links;
  };

  if (!members.empty()) {
    set_status(QStringLiteral("Extracting %1 archive member(s)…").arg(members.size()));
  }
  const auto cache_root = archive_member_cache_root("dirtoo-archive-drop");
  // Extraction, the drop-onto-itself checks and symlink creation all touch
  // (possibly slow) drives — run them on the I/O pool.
  run_io(
      this,
      [plain, members, dest_dir, link, cache_root]() mutable {
        DropPlan plan;
        plan.sources = std::move(plain);
        for (const auto& m : members) {
          std::error_code ec;
          if (!m.extracted_candidate.empty() && std::filesystem::exists(m.extracted_candidate, ec)
              && !ec) {
            plan.sources.push_back(m.extracted_candidate);
            continue;
          }
          const auto dest = archive_member_dest_dir(cache_root, m.archive_file);
          if (auto extracted = ensure_archive_member_extracted(m.archive_file, m.member, dest)) {
            plan.sources.push_back(*extracted);
          }
        }
        if (plan.sources.empty()) {
          plan.refusal = QStringLiteral("Failed to extract archive member(s) for drop");
          return plan;
        }

        // Refuse dropping a selection into itself / a selected folder.
        const auto dest_path = dest_dir.lexically_normal();
        for (const auto& src : plan.sources) {
          std::error_code ec;
          if (std::filesystem::equivalent(src, dest_path, ec)) {
            plan.refusal = QStringLiteral("Cannot drop an item onto itself");
            return plan;
          }
          if (std::filesystem::is_directory(src, ec)) {
            const auto rel = dest_path.lexically_relative(src.lexically_normal());
            if (!rel.empty() && *rel.begin() != "..") {
              plan.refusal = QStringLiteral("Cannot drop into a selected folder");
              return plan;
            }
          }
        }

        if (link) {
          plan.linked = true;
          for (const auto& src : plan.sources) {
            const auto target = dest_dir / src.filename();
            auto result = dirops::create_symlink(src, target);
            plan.links.push_back(LinkOutcome{
                src, target,
                result ? QString() : QString::fromStdString(result.error().to_string())});
          }
        }
        return plan;
      },
      [this, effective, dest_dir](DropPlan plan) {
        if (!plan.refusal.isEmpty()) {
          set_status(plan.refusal);
          return;
        }
        if (plan.linked) {
          // History is GUI-thread state — record here, not on the worker.
          int ok = 0;
          for (const auto& l : plan.links) {
            operations_history().record_simple(OperationKind::Symlink, {l.source}, l.link,
                                               l.error.isEmpty(), l.error);
            ok += l.error.isEmpty() ? 1 : 0;
          }
          set_status(QStringLiteral("Linked %1 (%2 failed)")
                         .arg(ok)
                         .arg(static_cast<int>(plan.links.size()) - ok));
          on_directory_changed();
          return;
        }

        TransferRequest req;
        req.mode = (effective == Qt::MoveAction) ? ClipboardMode::Cut : ClipboardMode::Copy;
        req.destination_directory = dest_dir;
        for (const auto& src : plan.sources) {
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

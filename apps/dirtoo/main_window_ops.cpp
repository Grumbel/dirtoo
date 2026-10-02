// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include "clipboard.hpp"
#include "path_display.hpp"
#include "mutation_support.hpp"
#include "conflict_dialog.hpp"
#include "name_input_dialog.hpp"
#include "operations_history.hpp"
#include "activity_monitor.hpp"
#include "async_io.hpp"
#include "dirops/ops.hpp"
#include "dirops/util.hpp"
#include <QMimeData>
#include <QPointer>
#include <filesystem>

namespace dirtoo::app {

// All filesystem mutations below run on io_thread_pool() via run_mutation():
// the target may be a USB or network drive that takes tens of seconds to
// answer, and even a single rename/mkdir/exists() must not freeze the GUI.

namespace {

struct MutationStep {
  OperationKind kind = OperationKind::Rename;
  std::vector<std::filesystem::path> sources;
  std::filesystem::path destination;
  bool ok = false;
  QString error;
};

/// Worker result: either a name clash (nothing changed yet) or the steps done.
struct MutationResult {
  bool conflict = false;
  ConflictProbe probe; ///< destination metadata when conflict
  std::vector<MutationStep> steps;

  [[nodiscard]] QString first_error() const
  {
    for (const auto& st : steps) {
      if (!st.ok) {
        return st.error;
      }
    }
    return {};
  }
};

[[nodiscard]] MutationStep make_step(OperationKind kind,
                                     std::vector<std::filesystem::path> sources,
                                     std::filesystem::path destination,
                                     const dirops::OpResult& result)
{
  MutationStep step;
  step.kind = kind;
  step.sources = std::move(sources);
  step.destination = std::move(destination);
  step.ok = result.has_value();
  if (!result) {
    step.error = QString::fromStdString(result.error().to_string());
  }
  return step;
}

[[nodiscard]] MutationResult single_step(MutationStep step)
{
  MutationResult r;
  r.steps.push_back(std::move(step));
  return r;
}

[[nodiscard]] bool path_exists(const std::filesystem::path& p)
{
  std::error_code ec;
  return std::filesystem::exists(std::filesystem::symlink_status(p, ec)) && !ec;
}

/// Run @p work (returns MutationResult) on the I/O pool. The continuation is
/// bound to qApp, not the window, so every step is written to the operations
/// history even if the window was closed while the drive was busy; @p done
/// then receives the window, or nullptr if it is gone.
template <typename Work, typename Done>
void run_mutation(MainWindow* window, const QString& activity, Work&& work, Done&& done)
{
  const QString job = ActivityMonitor::instance().begin_job(QStringLiteral("fs-op"), activity);
  QPointer<MainWindow> self(window);
  run_io(qApp, std::forward<Work>(work),
         [job, self, done = std::forward<Done>(done)](MutationResult result) {
           ActivityMonitor::instance().end_job(job);
           for (const auto& st : result.steps) {
             operations_history().record_simple(st.kind, st.sources, st.destination, st.ok,
                                                st.error);
           }
           done(self.data(), result);
         });
}

} // namespace

void MainWindow::set_clipboard(ClipboardMode mode)
{
  const auto selected = selected_fileinfos();
  if (selected.empty()) {
    set_status(QStringLiteral("Nothing selected"));
    return;
  }
  const auto paths = paths_from_fileinfos(selected);
  apply_paths_to_system_clipboard(mode, paths);
  set_status(QStringLiteral("%1 item(s) %2").arg(paths.size()).arg(clipboard_mode_verb(mode)));
  update_edit_actions();
}

void MainWindow::on_copy()
{
  set_clipboard(ClipboardMode::Copy);
}

bool MainWindow::ensure_mutations_allowed()
{
  if (!read_only_) {
    return true;
  }
  set_status(QStringLiteral("Read-only mode: filesystem changes are disabled"));
  return false;
}

void MainWindow::update_mutation_actions()
{
  const bool allow = !read_only_ && !transfer_controller_.busy();
  if (paste_act_ != nullptr) {
    paste_act_->setEnabled(allow
                           && clipboard_has_paths(QApplication::clipboard()->mimeData()));
  }
  // Other mutation actions are created as toolbar/menu items without dedicated
  // members; they remain clickable but handlers call ensure_mutations_allowed().
  if (read_only_act_ != nullptr) {
    read_only_act_->setChecked(read_only_);
  }
  update_window_title();
}

void MainWindow::update_window_title()
{
  const WindowTitleTexts titles = make_window_title_texts(location_, read_only_);
  setWindowTitle(titles.window_title);
  setWindowIconText(titles.icon_text);
}

void MainWindow::on_toggle_read_only(bool checked)
{
  read_only_ = checked;
  update_mutation_actions();
  set_status(read_only_ ? QStringLiteral("Read-only mode on")
                        : QStringLiteral("Read-only mode off"));
}

void MainWindow::on_cut()
{
  if (!ensure_mutations_allowed()) {
    return;
  }

  set_clipboard(ClipboardMode::Cut);
}

void MainWindow::on_paste()
{
  if (!ensure_mutations_allowed()) {
    return;
  }

  if (!location_allows_filesystem_mutations(location_)) {
    set_status(QStringLiteral("Read-only: cannot modify this location"));
    return;
  }

  if (transfer_controller_.busy()) {
    return;
  }

  const ClipboardPayload payload = parse_clipboard_mime(QApplication::clipboard()->mimeData());
  if (payload.paths.empty()) {
    set_status(QStringLiteral("Clipboard has no files"));
    return;
  }

  if (payload.mode == ClipboardMode::Link) {
    on_paste_link();
    return;
  }

  TransferRequest req;
  req.mode = payload.mode;
  req.destination_directory = location_.as_path();
  req.sources = payload.paths;
  start_transfer(req);
}

void MainWindow::on_paste_link()
{
  if (!ensure_mutations_allowed()) {
    return;
  }

  if (!location_allows_filesystem_mutations(location_)) {
    set_status(QStringLiteral("Read-only: cannot modify this location"));
    return;
  }
  const ClipboardPayload payload = parse_clipboard_mime(QApplication::clipboard()->mimeData());
  if (payload.paths.empty()) {
    // Allow "Paste as Link" using whatever paths are on the clipboard.
    set_status(QStringLiteral("Clipboard has no files"));
    return;
  }
  const auto dest_dir = location_.as_path();
  const auto sources = payload.paths;
  set_status(QStringLiteral("Linking %1 item(s)…").arg(sources.size()));
  run_mutation(
      this, QStringLiteral("Creating links…"),
      [dest_dir, sources] {
        MutationResult r;
        for (const auto& src : sources) {
          const auto dest = dest_dir / src.filename();
          r.steps.push_back(
              make_step(OperationKind::Symlink, {src}, dest, dirops::create_symlink(src, dest)));
        }
        return r;
      },
      [](MainWindow* self, const MutationResult& r) {
        if (self == nullptr) {
          return;
        }
        int ok = 0;
        for (const auto& st : r.steps) {
          if (st.ok) {
            ++ok;
          } else if (self->message_area_ != nullptr) {
            self->message_area_->show_error(st.error);
          }
        }
        self->set_status(QStringLiteral("Linked %1 (%2 failed)")
                             .arg(ok)
                             .arg(static_cast<int>(r.steps.size()) - ok));
        self->on_directory_changed();
      });
}

void MainWindow::on_mkdir()
{
  if (!ensure_mutations_allowed()) {
    return;
  }

  if (!location_allows_filesystem_mutations(location_)) {
    set_status(QStringLiteral("Read-only: cannot modify this location"));
    return;
  }

  const auto name_opt = ask_item_name(this, QStringLiteral("New Folder"),
                                      QStringLiteral("Folder name:"),
                                      QStringLiteral("New Folder"),
                                      QStringLiteral("Create"));
  if (!name_opt || name_opt->isEmpty()) {
    return;
  }
  const QString name = *name_opt;
  const auto dest = location_.as_path() / name.toStdString();

  // Phase 1: create unless the name is taken (then report the clash).
  run_mutation(
      this, QStringLiteral("Creating folder…"),
      [dest] {
        if (path_exists(dest)) {
          MutationResult r;
          r.conflict = true;
          r.probe = probe_conflict({}, dest);
          return r;
        }
        return single_step(
            make_step(OperationKind::Mkdir, {}, dest, dirops::create_directory(dest)));
      },
      [name, dest](MainWindow* self, const MutationResult& r) {
        if (self == nullptr) {
          return;
        }
        if (!r.conflict) {
          self->finish_simple_mutation(QStringLiteral("New Folder"), r.first_error());
          return;
        }
        // Phase 2: user decides. Replace is disabled by the dialog when the
        // existing entry is a folder (would delete the whole tree).
        const auto chosen = ask_conflict_policy(self, name, {}, dest, r.probe);
        if (!chosen || chosen->policy == dirops::ConflictPolicy::Skip
            || chosen->policy == dirops::ConflictPolicy::Fail) {
          return;
        }
        const bool overwrite = chosen->policy == dirops::ConflictPolicy::Overwrite;
        run_mutation(
            self, QStringLiteral("Creating folder…"),
            [dest, overwrite] {
              MutationResult r2;
              auto target = dest;
              if (overwrite) {
                r2.steps.push_back(
                    make_step(OperationKind::Delete, {dest}, {}, dirops::remove_path(dest)));
                if (!r2.steps.back().ok) {
                  return r2;
                }
              } else {
                target = dirops::unique_path(dest);
              }
              r2.steps.push_back(make_step(OperationKind::Mkdir, {}, target,
                                           dirops::create_directory(target)));
              return r2;
            },
            [](MainWindow* self2, const MutationResult& r2) {
              if (self2 != nullptr) {
                self2->finish_simple_mutation(QStringLiteral("New Folder"), r2.first_error());
              }
            });
      });
}

void MainWindow::on_create_file()
{
  if (!ensure_mutations_allowed()) {
    return;
  }

  if (!location_allows_filesystem_mutations(location_)) {
    set_status(QStringLiteral("Read-only: cannot modify this location"));
    return;
  }

  const auto name_opt = ask_item_name(this, QStringLiteral("New File"),
                                      QStringLiteral("File name:"),
                                      QStringLiteral("New File"),
                                      QStringLiteral("Create"));
  if (!name_opt || name_opt->isEmpty()) {
    return;
  }
  const auto desired = location_.as_path() / name_opt->toStdString();
  run_mutation(
      this, QStringLiteral("Creating file…"),
      [desired] {
        const auto dest = path_exists(desired) ? dirops::unique_path(desired) : desired;
        return single_step(
            make_step(OperationKind::Mkfile, {}, dest, dirops::create_file(dest)));
      },
      [](MainWindow* self, const MutationResult& r) {
        if (self != nullptr) {
          self->finish_simple_mutation(QStringLiteral("New File"), r.first_error());
        }
      });
}

void MainWindow::on_swap_names()
{
  if (!ensure_mutations_allowed()) {
    return;
  }
  if (!location_allows_filesystem_mutations(location_)) {
    set_status(QStringLiteral("Read-only: cannot modify this location"));
    return;
  }
  const auto selected = selected_fileinfos();
  if (selected.size() != 2) {
    set_status(QStringLiteral("Select exactly two items to swap names"));
    return;
  }
  const auto a = selected[0].path();
  const auto b = selected[1].path();
  run_mutation(
      this, QStringLiteral("Swapping names…"),
      [a, b] {
        return single_step(make_step(OperationKind::Swap, {a, b}, {}, dirops::swap_names(a, b)));
      },
      [](MainWindow* self, const MutationResult& r) {
        if (self != nullptr) {
          self->finish_simple_mutation(QStringLiteral("Swap Names"), r.first_error());
        }
      });
}

void MainWindow::on_rename_selected()
{
  if (!ensure_mutations_allowed()) {
    return;
  }
  if (!location_allows_filesystem_mutations(location_)) {
    set_status(QStringLiteral("Read-only: cannot modify this location"));
    return;
  }

  const auto selected = selected_fileinfos();
  if (selected.size() != 1) {
    set_status(QStringLiteral("Select exactly one item to rename"));
    return;
  }

  const auto& fi = selected.front();
  const auto name_opt = ask_item_name(this, QStringLiteral("Rename"),
                                      QStringLiteral("New name:"),
                                      QString::fromStdString(fi.basename()),
                                      QStringLiteral("Rename"));
  if (!name_opt || name_opt->isEmpty()) {
    return;
  }
  const QString name = *name_opt;
  const auto src = fi.path();
  const auto dest = src.parent_path() / name.toStdString();

  auto do_rename = [src, dest](dirops::Options opt) {
    return single_step(
        make_step(OperationKind::Rename, {src}, dest, dirops::rename_path(src, dest, opt)));
  };
  auto finish = [](MainWindow* self, const MutationResult& r) {
    if (self != nullptr) {
      self->finish_simple_mutation(QStringLiteral("Rename"), r.first_error());
    }
  };

  run_mutation(
      this, QStringLiteral("Renaming…"),
      [src, dest, do_rename] {
        if (dest != src && path_exists(dest)) {
          MutationResult r;
          r.conflict = true;
          r.probe = probe_conflict(src, dest);
          return r;
        }
        return do_rename(dirops::Options{});
      },
      [name, src, dest, do_rename, finish](MainWindow* self, const MutationResult& r) {
        if (self == nullptr) {
          return;
        }
        if (!r.conflict) {
          finish(self, r);
          return;
        }
        const auto chosen = ask_conflict_policy(self, name, src, dest, r.probe);
        if (!chosen) {
          return;
        }
        dirops::Options opt;
        opt.conflict = chosen->policy;
        run_mutation(self, QStringLiteral("Renaming…"), [do_rename, opt] { return do_rename(opt); },
                     finish);
      });
}

void MainWindow::on_delete_selected()
{
  if (!ensure_mutations_allowed()) {
    return;
  }

  if (!location_allows_filesystem_mutations(location_)) {
    set_status(QStringLiteral("Read-only: cannot modify this location"));
    return;
  }

  const auto selected = selected_fileinfos();
  if (selected.empty()) {
    return;
  }

  const QString msg = selected.size() == 1
                          ? QStringLiteral("Delete “%1”?")
                                .arg(QString::fromStdString(selected.front().basename()))
                          : QStringLiteral("Delete %1 items?").arg(selected.size());
  if (QMessageBox::question(this, QStringLiteral("Delete"), msg) != QMessageBox::Yes) {
    return;
  }

  const auto paths = paths_from_fileinfos(selected);
  set_status(QStringLiteral("Deleting %1 item(s)…").arg(paths.size()));
  run_mutation(
      this, QStringLiteral("Deleting %1 item(s)…").arg(paths.size()),
      [paths] {
        MutationResult r;
        for (const auto& p : paths) {
          r.steps.push_back(make_step(OperationKind::Delete, {p}, {}, dirops::remove_path(p)));
          if (!r.steps.back().ok) {
            break; // stop at the first failure, as before
          }
        }
        return r;
      },
      [](MainWindow* self, const MutationResult& r) {
        if (self != nullptr) {
          self->finish_simple_mutation(QStringLiteral("Delete"), r.first_error());
        }
      });
}

void MainWindow::finish_simple_mutation(const QString& title, const QString& error)
{
  if (!error.isEmpty()) {
    QMessageBox::warning(this, title, error);
  }
  on_directory_changed();
}

} // namespace dirtoo::app

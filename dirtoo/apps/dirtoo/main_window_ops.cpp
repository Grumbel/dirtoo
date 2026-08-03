// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"

#include "clipboard.hpp"
#include "conflict_dialog.hpp"
#include "name_input_dialog.hpp"
#include "operations_history.hpp"
#include "message_area.hpp"

#include "dirops/ops.hpp"
#include "dirops/util.hpp"

#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QMimeData>

#include <filesystem>

namespace dirtoo::app {

void MainWindow::set_clipboard(ClipboardMode mode)
{
  const auto selected = selected_fileinfos();
  if (selected.empty()) {
    set_status(QStringLiteral("Nothing selected"));
    return;
  }
  std::vector<std::filesystem::path> paths;
  paths.reserve(selected.size());
  for (const auto& fi : selected) {
    paths.push_back(fi.path());
  }
  QApplication::clipboard()->setMimeData(make_clipboard_mime(mode, paths));
  QString verb = QStringLiteral("copied");
  if (mode == ClipboardMode::Cut) {
    verb = QStringLiteral("cut");
  } else if (mode == ClipboardMode::Link) {
    verb = QStringLiteral("marked for link");
  }
  set_status(QStringLiteral("%1 item(s) %2").arg(paths.size()).arg(verb));
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
  const QString title = read_only_ ? QStringLiteral("dirtoo [read-only]")
                                   : QStringLiteral("dirtoo");
  setWindowTitle(title);
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

  if (location_.is_archive()) {
    set_status(QStringLiteral("Read-only: browsing inside an archive"));
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

  if (location_.is_archive()) {
    set_status(QStringLiteral("Read-only: browsing inside an archive"));
    return;
  }
  const ClipboardPayload payload = parse_clipboard_mime(QApplication::clipboard()->mimeData());
  if (payload.paths.empty()) {
    // Allow "Paste as Link" using whatever paths are on the clipboard.
    set_status(QStringLiteral("Clipboard has no files"));
    return;
  }
  int ok = 0;
  int fail = 0;
  for (const auto& src : payload.paths) {
    const auto dest = location_.as_path() / src.filename();
    auto result = dirops::create_symlink(src, dest);
    if (result) {
      ++ok;
      operations_history().record_simple(OperationKind::Symlink, {src}, dest, true);
    } else {
      ++fail;
      operations_history().record_simple(OperationKind::Symlink, {src}, dest, false,
                                         QString::fromStdString(result.error().to_string()));
      if (message_area_ != nullptr) {
        message_area_->show_error(QString::fromStdString(result.error().to_string()));
      }
    }
  }
  set_status(QStringLiteral("Linked %1 (%2 failed)").arg(ok).arg(fail));
  on_directory_changed();
}

void MainWindow::on_mkdir()
{
  if (!ensure_mutations_allowed()) {
    return;
  }

  if (location_.is_archive()) {
    set_status(QStringLiteral("Read-only: browsing inside an archive"));
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
  if (std::filesystem::exists(dest)) {
    const auto chosen = ask_conflict_policy(this, name);
    if (!chosen || chosen->policy == dirops::ConflictPolicy::Skip) {
      return;
    }
    if (chosen->policy == dirops::ConflictPolicy::Overwrite) {
      auto rm = dirops::remove_path(dest);
      if (!rm) {
        QMessageBox::warning(this, QStringLiteral("New Folder"),
                             QString::fromStdString(rm.error().to_string()));
        return;
      }
    } else if (chosen->policy == dirops::ConflictPolicy::Rename) {
      const auto unique =
          dest.parent_path() / (dest.stem().string() + " (2)" + dest.extension().string());
      auto result = dirops::create_directory(unique);
      if (!result) {
        QMessageBox::warning(this, QStringLiteral("New Folder"),
                             QString::fromStdString(result.error().to_string()));
      }
      on_directory_changed();
      return;
    }
  }

  auto result = dirops::create_directory(dest);
  if (!result) {
    operations_history().record_simple(OperationKind::Mkdir, {}, dest, false,
                                       QString::fromStdString(result.error().to_string()));
    QMessageBox::warning(this, QStringLiteral("New Folder"),
                         QString::fromStdString(result.error().to_string()));
    return;
  }
  operations_history().record_simple(OperationKind::Mkdir, {}, dest, true);
  on_directory_changed();
}

void MainWindow::on_create_file()
{
  if (!ensure_mutations_allowed()) {
    return;
  }

  if (location_.is_archive()) {
    set_status(QStringLiteral("Read-only: browsing inside an archive"));
    return;
  }

  const auto name_opt = ask_item_name(this, QStringLiteral("New File"),
                                      QStringLiteral("File name:"),
                                      QStringLiteral("New File"),
                                      QStringLiteral("Create"));
  if (!name_opt || name_opt->isEmpty()) {
    return;
  }
  const QString name = *name_opt;
  auto dest = location_.as_path() / name.toStdString();
  if (std::filesystem::exists(dest)) {
    const auto unique = dirops::unique_path(dest);
    dest = unique;
  }
  auto result = dirops::create_file(dest);
  if (!result) {
    operations_history().record_simple(OperationKind::Mkfile, {}, dest, false,
                                       QString::fromStdString(result.error().to_string()));
    QMessageBox::warning(this, QStringLiteral("New File"),
                         QString::fromStdString(result.error().to_string()));
    return;
  }
  operations_history().record_simple(OperationKind::Mkfile, {}, dest, true);
  on_directory_changed();
}

void MainWindow::on_swap_names()
{
  if (!ensure_mutations_allowed()) {
    return;
  }
  if (location_.is_archive()) {
    set_status(QStringLiteral("Read-only: browsing inside an archive"));
    return;
  }
  const auto selected = selected_fileinfos();
  if (selected.size() != 2) {
    set_status(QStringLiteral("Select exactly two items to swap names"));
    return;
  }
  auto result = dirops::swap_names(selected[0].path(), selected[1].path());
  if (!result) {
    operations_history().record_simple(
        OperationKind::Swap, {selected[0].path(), selected[1].path()}, {}, false,
        QString::fromStdString(result.error().to_string()));
    QMessageBox::warning(this, QStringLiteral("Swap Names"),
                         QString::fromStdString(result.error().to_string()));
    return;
  }
  operations_history().record_simple(OperationKind::Swap,
                                     {selected[0].path(), selected[1].path()}, {}, true);
  on_directory_changed();
}

void MainWindow::on_rename_selected()
{
  if (!ensure_mutations_allowed()) {
    return;
  }
  if (location_.is_archive()) {
    set_status(QStringLiteral("Read-only: browsing inside an archive"));
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

  const auto dest = fi.path().parent_path() / name.toStdString();
  dirops::Options opt;
  if (std::filesystem::exists(dest) && dest != fi.path()) {
    const auto chosen = ask_conflict_policy(this, name);
    if (!chosen) {
      return;
    }
    opt.conflict = chosen->policy;
  }

  auto result = dirops::rename_path(fi.path(), dest, opt);
  if (!result) {
    operations_history().record_simple(OperationKind::Rename, {fi.path()}, dest, false,
                                       QString::fromStdString(result.error().to_string()));
    QMessageBox::warning(this, QStringLiteral("Rename"),
                         QString::fromStdString(result.error().to_string()));
    return;
  }
  operations_history().record_simple(OperationKind::Rename, {fi.path()}, dest, true);
  on_directory_changed();
}

void MainWindow::on_delete_selected()
{
  if (!ensure_mutations_allowed()) {
    return;
  }

  if (location_.is_archive()) {
    set_status(QStringLiteral("Read-only: browsing inside an archive"));
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

  for (const auto& fi : selected) {
    auto result = dirops::remove_path(fi.path());
    if (!result) {
      operations_history().record_simple(OperationKind::Delete, {fi.path()}, {}, false,
                                         QString::fromStdString(result.error().to_string()));
      QMessageBox::warning(this, QStringLiteral("Delete"),
                           QString::fromStdString(result.error().to_string()));
      break;
    }
    operations_history().record_simple(OperationKind::Delete, {fi.path()}, {}, true);
  }
  on_directory_changed();
}

} // namespace dirtoo::app

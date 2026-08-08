// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include "clipboard.hpp"
#include "conflict_dialog.hpp"
#include "name_input_dialog.hpp"
#include "operations_history.hpp"
#include "dirops/ops.hpp"
#include "dirops/util.hpp"
#include <QMimeData>
#include <filesystem>

namespace dirtoo::app {

namespace {

/// Collapse intermediate path segments when the path is long.
/// Example: /usr/local/bin/tool -> /u…/l…/b…/tool (basename always full).
/// Leaves the path unchanged when it already fits max_chars.
QString elide_path_for_title(QString path, int max_chars = 96)
{
  path.replace(QLatin1Char('\\'), QLatin1Char('/'));
  if (path.size() <= max_chars || max_chars < 8) {
    return path;
  }

  // Preserve a trailing slash meaning "directory root style" only if path is "/".
  const bool absolute = path.startsWith(QLatin1Char('/'));
  QStringList parts = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
  // Absolute "/a/b" -> ["", "a", "b"]; collapse only non-empty intermediate parts.
  if (parts.isEmpty()) {
    return path;
  }

  auto joined = [&]() {
    QString out;
    for (int i = 0; i < parts.size(); ++i) {
      if (i > 0) {
        out += QLatin1Char('/');
      }
      out += parts[i];
    }
    // split("a") on non-absolute keeps single element without leading slash — fine.
    if (absolute && !out.startsWith(QLatin1Char('/'))) {
      out.prepend(QLatin1Char('/'));
    }
    return out;
  };

  // Indices of collapsible segments: non-empty, not the last non-empty component.
  int last_nonempty = -1;
  for (int i = parts.size() - 1; i >= 0; --i) {
    if (!parts[i].isEmpty()) {
      last_nonempty = i;
      break;
    }
  }
  if (last_nonempty < 0) {
    return path;
  }

  auto is_collapsed = [](const QString& s) {
    return s.size() >= 2 && s.endsWith(QChar(0x2026)); // …
  };

  // Prefer collapsing leftmost long segments first (keep basename + nearby parents readable longer).
  while (joined().size() > max_chars) {
    int victim = -1;
    for (int i = 0; i < last_nonempty; ++i) {
      if (parts[i].isEmpty()) {
        continue;
      }
      if (!is_collapsed(parts[i]) && parts[i].size() > 1) {
        victim = i;
        break;
      }
    }
    if (victim < 0) {
      // Everything intermediate already minimal; hard-trim from the left of the string.
      QString j = joined();
      if (j.size() <= max_chars) {
        return j;
      }
      // Keep basename region: "…/basename"
      const QString base = parts[last_nonempty];
      const QString suffix = QLatin1Char('/') + base;
      const int keep = max_chars - 1; // for leading …
      if (keep <= suffix.size()) {
        return QChar(0x2026) + base.right(std::max(1, max_chars - 1));
      }
      return QChar(0x2026) + j.right(max_chars - 1);
    }
    // First character + ellipsis (… U+2026).
    const QChar head = parts[victim].at(0);
    parts[victim] = QString(head) + QChar(0x2026);
  }
  return joined();
}

QString location_display_path(const dirtoo::fs::Location& loc)
{
  if (loc.empty()) {
    return QString();
  }
  if (loc.is_archive()) {
    return QString::fromStdString(loc.as_url());
  }
  return QString::fromStdString(loc.as_path().string());
}

} // namespace


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
  update_window_title();
}

void MainWindow::update_window_title()
{
  const QString path = location_display_path(location_);
  // Title bar has room for a longer path; taskbar/icon labels often ~20 chars.
  const QString title_path =
      path.isEmpty() ? QString() : elide_path_for_title(path, /*max_chars=*/96);
  const QString icon_path =
      path.isEmpty() ? QString() : elide_path_for_title(path, /*max_chars=*/20);

  QString title;
  if (!title_path.isEmpty()) {
    title = title_path + QStringLiteral(" — dirtoo");
  } else {
    title = QStringLiteral("dirtoo");
  }
  if (read_only_) {
    title += QStringLiteral(" [read-only]");
  }
  setWindowTitle(title);

  // Separate minimized/taskbar name where the WM supports WM_ICON_NAME.
  // Keep path-first; drop the " — dirtoo" suffix so the ~20 char budget is path.
  QString icon;
  if (!icon_path.isEmpty()) {
    icon = icon_path;
    if (read_only_) {
      icon += QStringLiteral(" [ro]");
    }
  } else {
    icon = read_only_ ? QStringLiteral("dirtoo [ro]") : QStringLiteral("dirtoo");
  }
  setWindowIconText(icon);
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

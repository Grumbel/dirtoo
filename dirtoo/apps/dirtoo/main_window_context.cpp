// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include "file_context_menu.hpp"
#include "clipboard.hpp"
#include "properties_dialog.hpp"
#include "open_with.hpp"

#include <QPoint>

namespace dirtoo::app {

void MainWindow::on_context_menu(const QPoint& pos)
{
  auto* view = current_view();
  const bool graphics = (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr);
  if (view == nullptr && !graphics) {
    return;
  }

  // Resolve the item under the pointer (if any). Empty background → directory menu.
  QModelIndex under;
  if (graphics && graphics_view_ != nullptr) {
    under = graphics_view_->index_at(pos);
    if (under.isValid()) {
      const auto rows = graphics_view_->selected_rows();
      bool already = false;
      for (int r : rows) {
        if (r == under.row()) {
          already = true;
          break;
        }
      }
      if (!already) {
        graphics_view_->select_row(under.row(), true);
      }
    }
  } else if (view != nullptr && view->selectionModel() != nullptr) {
    const QPoint vp = view->viewport()->mapFrom(view, pos);
    under = view->indexAt(vp);
    if (under.isValid() && !view->selectionModel()->isSelected(under)) {
      view->selectionModel()->select(
          under, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
      view->setCurrentIndex(under);
    }
  }

  FileContextMenuCallbacks cb;
  cb.current_location = location_;
  cb.mkdir = [this] { on_mkdir(); };
  cb.create_file = [this] { on_create_file(); };
  cb.paste = [this] { on_paste(); };
  cb.select_all = [this] { on_select_all(); };
  cb.cut = [this] { on_cut(); };
  cb.copy = [this] { on_copy(); };
  cb.delete_selected = [this] { on_delete_selected(); };
  cb.rename_selected = [this] { on_rename_selected(); };
  cb.checksums_selected = [this] { on_checksums(); };
  cb.tag_selected = [this] { on_tag_selected(); };
  cb.mark_opened = [this] { on_mark_selection_opened(); };
  cb.mark_unopened = [this] { on_mark_selection_unopened(); };
  cb.properties_selected = [this] { on_properties(); };
  cb.reload_thumbnails = [this] { on_reload_thumbnails(); };
  cb.prepare_thumbnails = [this] { on_prepare_thumbnails(); };
  cb.make_directory_thumbnails = [this] { on_make_directory_thumbnails(); };
  cb.open_location = [this](const fs::Location& loc) { open_location(loc); };
  cb.open_location_new_window = [this](const fs::Location& loc) {
    auto* win = new MainWindow();
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->show();
    win->open_location(loc);
  };
  cb.open_terminal = [this](const std::filesystem::path& dir) {
    if (!open_in_terminal(dir)) {
      set_status(QStringLiteral("Could not launch a terminal emulator"));
    }
  };
  cb.paste_into = [this](const std::filesystem::path& dest) {
    const ClipboardPayload payload =
        parse_clipboard_mime(QApplication::clipboard()->mimeData());
    if (payload.paths.empty()) {
      set_status(QStringLiteral("Clipboard has no files"));
      return;
    }
    TransferRequest req;
    req.mode = payload.mode;
    req.sources = payload.paths;
    req.destination_directory = dest;
    start_transfer(req);
  };
  cb.set_status = [this](const QString& s) { set_status(s); };
  cb.show_properties = [this](const std::vector<fs::FileInfo>& items) {
    show_properties_dialog(this, items);
  };

  const QPoint global = graphics ? graphics_view_->mapToGlobal(pos)
                                 : view->viewport()->mapToGlobal(pos);
  if (!under.isValid()) {
    exec_directory_context_menu(this, global, cb);
  } else {
    exec_item_context_menu(this, global, selected_fileinfos(), cb);
  }
}

} // namespace dirtoo::app

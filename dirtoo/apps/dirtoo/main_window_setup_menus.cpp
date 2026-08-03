// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"

#include "badge_icons.hpp"
#include "location_icons.hpp"
#include "location_url.hpp"
#include "location_menu_helpers.hpp"
#include "file_views.hpp"
#include "file_context_menu.hpp"
#include "file_item_delegate.hpp"
#include "devices_controller.hpp"
#include "udisks_client.hpp"
#include "about_dialog.hpp"
#include "open_history.hpp"
#include "operations_history.hpp"
#include "preferences_dialog.hpp"
#include "open_with.hpp"
#include <QActionGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenuBar>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QKeySequence>
#include <QPushButton>

namespace dirtoo::app {

void MainWindow::setup_menus()
{
  // Menu bar
  {
  auto* file_menu = menuBar()->addMenu(QStringLiteral("&File"));
  {
    auto* act = file_menu->addAction(theme_icon("window-new"), QStringLiteral("New Window"), this, &MainWindow::on_new_window);
    act->setShortcut(QKeySequence::New); // Ctrl+N
  }
  file_menu->addAction(theme_icon("folder-new"), QStringLiteral("New Folder…"), this, &MainWindow::on_mkdir);
  file_menu->addAction(theme_icon("document-new"), QStringLiteral("New File…"), this, &MainWindow::on_create_file);
  file_menu->addSeparator();
  {
    auto* act = file_menu->addAction(theme_icon("document-save-as"), QStringLiteral("Save File List As…"), this,
                                     &MainWindow::on_save_file_list);
    act->setShortcut(QKeySequence::SaveAs);
  }
  file_menu->addSeparator();
  {
    auto* act = file_menu->addAction(theme_icon("window-close"), QStringLiteral("Close"), this, &QWidget::close);
    act->setShortcut(QKeySequence::Close);
  }

  auto* edit_menu = menuBar()->addMenu(QStringLiteral("&Edit"));
  {
    auto* act = edit_menu->addAction(theme_icon("edit-cut"), QStringLiteral("Cut"), this, &MainWindow::on_cut);
    act->setShortcut(QKeySequence::Cut);
  }
  {
    auto* act = edit_menu->addAction(theme_icon("edit-copy"), QStringLiteral("Copy"), this, &MainWindow::on_copy);
    act->setShortcut(QKeySequence::Copy);
  }
  {
    paste_act_ = edit_menu->addAction(theme_icon("edit-paste"), QStringLiteral("Paste"), this, &MainWindow::on_paste);
    paste_act_->setShortcut(QKeySequence::Paste);
  }
  edit_menu->addAction(theme_icon("emblem-symbolic-link", "insert-link"), QStringLiteral("Paste as Link"), this, &MainWindow::on_paste_link);
  edit_menu->addAction(theme_icon("edit-copy"), QStringLiteral("Copy as Link"), this, [this] {
    set_clipboard(ClipboardMode::Link);
  });
  edit_menu->addSeparator();
  edit_menu->addAction(theme_icon("view-history", "document-open-recent"),
                       QStringLiteral("Operations History…"), this, [this] {
                         show_operations_history_dialog(this, [this](const QString& dir) {
                           open_location(fs::Location::from_path(dir.toStdString()));
                         });
                       });
  {
    auto* act = edit_menu->addAction(theme_icon("edit-select-all"), QStringLiteral("Select All"), this, &MainWindow::on_select_all);
    act->setShortcut(QKeySequence::SelectAll);
  }
  edit_menu->addSeparator();
  {
    auto* act = edit_menu->addAction(theme_icon("edit-rename", "document-edit"), QStringLiteral("Rename…"), this, &MainWindow::on_rename_selected);
    act->setShortcut(QKeySequence(Qt::Key_F2));
  }
  {
    auto* act = edit_menu->addAction(theme_icon("edit-delete"), QStringLiteral("Delete…"), this, &MainWindow::on_delete_selected);
    act->setShortcut(QKeySequence::Delete);
  }
  {
    auto* act = edit_menu->addAction(theme_icon("object-flip-horizontal", "edit-copy"), QStringLiteral("Swap Names"), this, &MainWindow::on_swap_names);
    act->setStatusTip(QStringLiteral("Exchange the names of exactly two selected items"));
  }
  {
    auto* act = edit_menu->addAction(theme_icon("document-properties"), QStringLiteral("Properties…"), this, &MainWindow::on_properties);
    act->setShortcut(QKeySequence(Qt::Key_F3));
  }
  edit_menu->addSeparator();
  {
    auto* act = edit_menu->addAction(theme_icon("preferences-system"), QStringLiteral("Preferences…"), this, &MainWindow::on_preferences);
    act->setShortcut(QKeySequence::Preferences);
  }

  auto* view_menu = menuBar()->addMenu(QStringLiteral("&View"));
  view_menu->addAction(icons_act_);
  view_menu->addAction(small_icons_act_);
  view_menu->addAction(detail_act_);
  view_menu->addSeparator();
  {
    auto* cols = view_menu->addMenu(QStringLiteral("Detail Columns"));
    struct ColOpt {
      const char* key;
      const char* label;
    };
    static constexpr ColOpt kCols[] = {
        {"size", "Size"},
        {"width", "Width"},
        {"height", "Height"},
        {"dimensions", "Dimensions"},
        {"aspectratio", "Aspect"},
        {"framerate", "FPS"},
        {"duration", "Duration"},
        {"modified", "Modified"},
        {"accessed", "Accessed"},
        {"changed", "Changed"},
        {"birth", "Created"},
        {"type", "Type"},
    };
    for (const auto& c : kCols) {
      auto* act = cols->addAction(QString::fromUtf8(c.label));
      act->setCheckable(true);
      const QString key = QString::fromUtf8(c.key);
      const bool default_off = key == QLatin1String("width") || key == QLatin1String("height")
                               || key == QLatin1String("accessed") || key == QLatin1String("changed")
                               || key == QLatin1String("birth");
      act->setChecked(detail_columns_.contains(key)
                      || (detail_columns_.isEmpty() && !default_off));
      connect(act, &QAction::toggled, this, [this, key](bool on) {
        if (on) {
          if (!detail_columns_.contains(key)) {
            detail_columns_.append(key);
          }
        } else {
          detail_columns_.removeAll(key);
        }
        apply_detail_column_visibility();
      });
    }
  }
  view_menu->addSeparator();
  if (show_hidden_act_ != nullptr) {
    view_menu->addAction(show_hidden_act_);
  }
  if (read_only_act_ != nullptr) {
    view_menu->addAction(read_only_act_);
  }
  {
    auto* act = view_menu->addAction(QStringLiteral("Show Full Paths"));
    act->setCheckable(true);
    act->setStatusTip(QStringLiteral("Show absolute paths instead of basenames in the Name column"));
    connect(act, &QAction::toggled, this, &MainWindow::on_toggle_show_abspath);
  }
  {
    auto* act = view_menu->addAction(theme_icon("chronometer", "appointment-soon"), QStringLiteral("Show Time Gaps"));
    act->setCheckable(true);
    act->setStatusTip(QStringLiteral("Show separators when consecutive items are far apart in modification time"));
    connect(act, &QAction::toggled, this, [this](bool on) {
      if (model_ != nullptr) {
        model_->set_show_timegaps(on);
        update_detail_row_heights();
      }
      if (tree_view_ != nullptr) {
        tree_view_->doItemsLayout();
      }
    });
  }
  view_menu->addSeparator();
  {
    auto* act = view_menu->addAction(theme_icon("view-sidetree", "view-list-tree"),
                                    QStringLiteral("Show Directory Tree"));
    act->setCheckable(true);
    act->setShortcut(QKeySequence(Qt::Key_F9));
    if (show_sidebar_act_ != nullptr) {
      act->setChecked(show_sidebar_act_->isChecked());
      connect(act, &QAction::toggled, show_sidebar_act_, &QAction::setChecked);
      connect(show_sidebar_act_, &QAction::toggled, act, &QAction::setChecked);
    }
  }
  view_menu->addSeparator();
  show_filter_act_ = view_menu->addAction(theme_icon("edit-find"), QStringLiteral("Show Filter"));
  show_filter_act_->setCheckable(true);
  show_filter_act_->setChecked(true);
  show_filter_act_->setShortcut(QKeySequence(QStringLiteral("Ctrl+K")));
  connect(show_filter_act_, &QAction::toggled, this, [this](bool on) {
    if (filter_row_ != nullptr) {
      filter_row_->setVisible(on);
    }
    // The line edit must stay visible whenever the row is shown (never hide it alone).
    if (filter_edit_ != nullptr) {
      filter_edit_->setVisible(true);
      filter_edit_->setEnabled(true);
    }
    if (on && filter_edit_ != nullptr) {
      filter_edit_->setFocus(Qt::ShortcutFocusReason);
    }
  });
  pin_filter_act_ = view_menu->addAction(theme_icon("pin", "object-locked"), QStringLiteral("Pin Filter"));
  pin_filter_act_->setCheckable(true);
  pin_filter_act_->setShortcut(QKeySequence(QStringLiteral("Ctrl+P")));
  connect(pin_filter_act_, &QAction::toggled, this, [this](bool on) {
    filter_pinned_ = on;
    if (on && show_filter_act_ != nullptr && !show_filter_act_->isChecked()) {
      show_filter_act_->setChecked(true);
    }
  });
  {
    auto* act = view_menu->addAction(theme_icon("go-jump"), QStringLiteral("Jump to…"), this, &MainWindow::on_show_leap);
    act->setShortcut(QKeySequence(QStringLiteral("/")));
  }
  {
    auto* act = view_menu->addAction(theme_icon("view-refresh"), QStringLiteral("Refresh"), this, &MainWindow::on_refresh);
    act->setShortcut(QKeySequence(Qt::Key_F5));
  }
  {
    auto* act = view_menu->addAction(theme_icon("view-refresh"), QStringLiteral("Reload Thumbnails"), this,
                                     &MainWindow::on_reload_thumbnails);
    act->setStatusTip(QStringLiteral("Clear and re-request thumbnails for the selection (or all visible)"));
  }
  {
    auto* act = view_menu->addAction(theme_icon("image-x-generic"), QStringLiteral("Prepare Thumbnails"), this,
                                     &MainWindow::on_prepare_thumbnails);
    act->setStatusTip(QStringLiteral("Request thumbnails for all visible items"));
  }
  {
    auto* act = view_menu->addAction(theme_icon("folder"), QStringLiteral("Make Directory Thumbnails"), this,
                                     &MainWindow::on_make_directory_thumbnails);
    act->setStatusTip(QStringLiteral("Build montage thumbnails for folders (selection or all visible)"));
  }
  {
    auto* act = view_menu->addAction(theme_icon("system-search"), QStringLiteral("Recursive Search…"), this,
                                     &MainWindow::on_show_search);
    act->setShortcut(QKeySequence(QStringLiteral("Ctrl+F")));
  }
  {
    auto* act = view_menu->addAction(theme_icon("zoom-in"), QStringLiteral("Zoom In"), this, &MainWindow::on_zoom_in);
    act->setShortcut(QKeySequence::ZoomIn);
  }
  {
    auto* act = view_menu->addAction(theme_icon("zoom-out"), QStringLiteral("Zoom Out"), this, &MainWindow::on_zoom_out);
    act->setShortcut(QKeySequence::ZoomOut);
  }
  {
    auto* act = view_menu->addAction(theme_icon("zoom-fit-best"), QStringLiteral("Crop Thumbnails"));
    act->setCheckable(true);
    act->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(act, &QAction::toggled, this, [this](bool on) {
      if (crop_thumbnails_act_ != nullptr) {
        crop_thumbnails_act_->setChecked(on);
      } else if (model_ != nullptr) {
        model_->set_crop_thumbnails(on);
      }
    });
    // Keep menu action in sync with toolbar.
    if (crop_thumbnails_act_ != nullptr) {
      connect(crop_thumbnails_act_, &QAction::toggled, act, &QAction::setChecked);
      act->setChecked(crop_thumbnails_act_->isChecked());
    }
  }
  {
    auto* act = view_menu->addAction(theme_icon("list-add"), QStringLiteral("More Icon Details"), this,
                                     &MainWindow::on_more_icon_details);
    act->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+=")));
  }
  {
    auto* act = view_menu->addAction(theme_icon("list-remove"), QStringLiteral("Less Icon Details"), this,
                                     &MainWindow::on_less_icon_details);
    act->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+-")));
  }
  view_menu->addSeparator();
  {
    auto* group_menu = view_menu->addMenu(QStringLiteral("Group By"));
    auto* group_actions = new QActionGroup(this);
    group_actions->setExclusive(true);
    auto add_group = [&](const QString& title, dirtoo::collection::GroupMode mode, bool checked) {
      auto* act = group_menu->addAction(title);
      act->setCheckable(true);
      act->setChecked(checked);
      group_actions->addAction(act);
      connect(act, &QAction::triggered, this, [this, mode] {
        collection_.set_group_mode(mode);
        update_detail_row_heights();
        {
          AppSettings s = load_settings();
          switch (mode) {
          case collection::GroupMode::Day:
            s.group_mode = QStringLiteral("day");
            break;
          case collection::GroupMode::Directory:
            s.group_mode = QStringLiteral("directory");
            break;
          case collection::GroupMode::Duration:
            s.group_mode = QStringLiteral("duration");
            break;
          default:
            s.group_mode = QStringLiteral("none");
            break;
          }
          save_settings(s);
        }
        if (model_ != nullptr) {
          model_->refresh();
        }
      });
    };
    add_group(QStringLiteral("None"), dirtoo::collection::GroupMode::None, true);
    add_group(QStringLiteral("Day"), dirtoo::collection::GroupMode::Day, false);
    add_group(QStringLiteral("Directory"), dirtoo::collection::GroupMode::Directory, false);
    add_group(QStringLiteral("Duration"), dirtoo::collection::GroupMode::Duration, false);
  }

  auto* sort_menu = menuBar()->addMenu(QStringLiteral("&Sort"));
  auto* sort_group = new QActionGroup(this);
  sort_group->setExclusive(true);
  auto add_sort = [&](const QString& title, dirtoo::collection::SortKey key, bool checked = false) {
    auto* act = sort_menu->addAction(title);
    act->setCheckable(true);
    act->setChecked(checked);
    sort_group->addAction(act);
    connect(act, &QAction::triggered, this, [this, key] {
      apply_sort_key(key, /*toggle_if_same=*/false);
    });
    return act;
  };
  add_sort(QStringLiteral("By Name"), dirtoo::collection::SortKey::Name, true);
  add_sort(QStringLiteral("By Size"), dirtoo::collection::SortKey::Size);
  add_sort(QStringLiteral("By Extension"), dirtoo::collection::SortKey::Extension);
  add_sort(QStringLiteral("By Date"), dirtoo::collection::SortKey::Modified);
  add_sort(QStringLiteral("By Type"), dirtoo::collection::SortKey::Type);
  sort_menu->addSeparator();
  add_sort(QStringLiteral("By Width"), dirtoo::collection::SortKey::Width);
  add_sort(QStringLiteral("By Height"), dirtoo::collection::SortKey::Height);
  add_sort(QStringLiteral("By Resolution"), dirtoo::collection::SortKey::Resolution);
  add_sort(QStringLiteral("By Aspect Ratio"), dirtoo::collection::SortKey::AspectRatio);
  add_sort(QStringLiteral("By Duration"), dirtoo::collection::SortKey::Duration);
  add_sort(QStringLiteral("By Framerate"), dirtoo::collection::SortKey::Framerate);
  sort_menu->addSeparator();
  add_sort(QStringLiteral("By Permissions"), dirtoo::collection::SortKey::Permissions);
  add_sort(QStringLiteral("Random Shuffle"), dirtoo::collection::SortKey::Random);
  sort_menu->addSeparator();
  {
    auto* act = sort_menu->addAction(theme_icon("folder"), QStringLiteral("Directories First"));
    act->setCheckable(true);
    act->setChecked(true);
    connect(act, &QAction::toggled, this, [this](bool on) {
      collection_.sorter().set_directories_first(on);
      {
        AppSettings s = load_settings();
        s.directories_first = on;
        save_settings(s);
      }
      request_async_sort();
    });
  }
  {
    auto* act = sort_menu->addAction(QStringLiteral("Reverse Order"));
    act->setCheckable(true);
    connect(act, &QAction::toggled, this, [this](bool on) {
      sort_ascending_ = !on;
      collection_.sorter().set_ascending(!on);
      request_async_sort();
    });
  }

  auto* go_menu = menuBar()->addMenu(QStringLiteral("&Go"));
  go_menu->addAction(theme_icon("go-previous"), QStringLiteral("Back"), this, &MainWindow::on_go_back);
  go_menu->addAction(theme_icon("go-next"), QStringLiteral("Forward"), this, &MainWindow::on_go_forward);
  go_menu->addAction(theme_icon("go-up"), QStringLiteral("Parent"), this, &MainWindow::on_go_parent);
  go_menu->addAction(theme_icon("window-new"), QStringLiteral("Parent in New Window"), this, &MainWindow::on_parent_new_window);
  go_menu->addAction(theme_icon("go-home"), QStringLiteral("Home"), this, &MainWindow::on_go_home);

  bookmarks_menu_ = new HistoryMenu(QStringLiteral("&Bookmarks"), this);
  menuBar()->addMenu(bookmarks_menu_);
  connect(bookmarks_menu_, &QMenu::aboutToShow, this, &MainWindow::on_rebuild_bookmarks_menu);

  history_menu_ = new HistoryMenu(QStringLiteral("H&istory"), this);
  menuBar()->addMenu(history_menu_);
  connect(history_menu_, &QMenu::aboutToShow, this, &MainWindow::on_rebuild_history_menu);

  recent_opens_menu_ = menuBar()->addMenu(QStringLiteral("Recently &Opened"));
  connect(recent_opens_menu_, &QMenu::aboutToShow, this, &MainWindow::on_rebuild_recent_opens_menu);

  auto* help_menu = menuBar()->addMenu(QStringLiteral("&Help"));
  help_menu->addAction(theme_icon("help-contents"), QStringLiteral("Filter expression help"), this,
                       &MainWindow::on_show_filter_help);
  help_menu->addAction(theme_icon("help-about"), QStringLiteral("About dirtoo"), this, &MainWindow::on_about);
  }


}

} // namespace dirtoo::app

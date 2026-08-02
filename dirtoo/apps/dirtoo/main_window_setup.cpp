#include "main_window.hpp"
#include "badge_icons.hpp"
#include "theme_icons.hpp"
#include "location_icons.hpp"
#include "location_url.hpp"
#include "location_menu_helpers.hpp"
#include "file_views.hpp"
#include "file_context_menu.hpp"
#include "file_item_delegate.hpp"
#include "devices_controller.hpp"
#include "directory_tree_model.hpp"
#include "udisks_client.hpp"
#include "about_dialog.hpp"
#include "open_history.hpp"
#include "operations_history.hpp"
#include "preferences_dialog.hpp"
#include "open_with.hpp"

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCompleter>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringListModel>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QKeySequence>
#include <QTimer>

namespace dirtoo::app {

void MainWindow::setup_background_workers()
{
  connect(&transfer_controller_, &TransferController::item_started, this,
          &MainWindow::on_transfer_item_started);
  connect(&transfer_controller_, &TransferController::byte_progress, this,
          &MainWindow::on_transfer_byte_progress);
  connect(&transfer_controller_, &TransferController::conflict_required, this,
          &MainWindow::on_transfer_conflict);
  connect(&transfer_controller_, &TransferController::finished, this,
          &MainWindow::on_transfer_finished);
  connect(&transfer_controller_, &TransferController::log_line, this, [this](const QString& line) {
    qInfo().noquote() << QStringLiteral("transfer: %1").arg(line);
    if (transfer_controller_.dialog() != nullptr) {
      transfer_controller_.dialog()->append_log(line);
    }
  });

  connect(&search_controller_, &SearchController::match_found, this, &MainWindow::on_search_match);
  connect(&search_controller_, &SearchController::progress, this, &MainWindow::on_search_progress);
  connect(&search_controller_, &SearchController::finished, this, &MainWindow::on_search_finished);

  dir_load_worker_ = new DirectoryLoadWorker;
  dir_load_thread_ = new QThread(this);
  dir_load_worker_->moveToThread(dir_load_thread_);
  connect(dir_load_thread_, &QThread::finished, dir_load_worker_, &QObject::deleteLater);
  connect(dir_load_worker_, &DirectoryLoadWorker::loaded, this, &MainWindow::on_directory_loaded);
  connect(dir_load_worker_, &DirectoryLoadWorker::failed, this, &MainWindow::on_directory_load_failed);
  qRegisterMetaType<std::vector<dirtoo::fs::FileInfo>>("std::vector<dirtoo::fs::FileInfo>");
  dir_load_thread_->start();

  sort_worker_ = new SortWorker;
  sort_thread_ = new QThread(this);
  sort_worker_->moveToThread(sort_thread_);
  connect(sort_thread_, &QThread::finished, sort_worker_, &QObject::deleteLater);
  connect(sort_worker_, &SortWorker::sorted, this, &MainWindow::on_sort_finished);
  qRegisterMetaType<dirtoo::collection::SortKey>("dirtoo::collection::SortKey");
  sort_thread_->start();

  filter_worker_ = new FilterWorker;
  filter_thread_ = new QThread(this);
  filter_worker_->moveToThread(filter_thread_);
  connect(filter_thread_, &QThread::finished, filter_worker_, &QObject::deleteLater);
  connect(filter_worker_, &FilterWorker::filtered, this, &MainWindow::on_filter_finished);
  qRegisterMetaType<dirtoo::collection::GroupMode>("dirtoo::collection::GroupMode");
  filter_thread_->start();

  dir_thumb_worker_ = new DirectoryThumbnailWorker;
  dir_thumb_thread_ = new QThread(this);
  dir_thumb_worker_->moveToThread(dir_thumb_thread_);
  connect(dir_thumb_thread_, &QThread::finished, dir_thumb_worker_, &QObject::deleteLater);
  connect(dir_thumb_worker_, &DirectoryThumbnailWorker::thumbnail_ready, this,
          &MainWindow::on_thumbnail_ready);
  connect(dir_thumb_worker_, &DirectoryThumbnailWorker::finished, this,
          [this](int ok, int fail) {
            set_status(QStringLiteral("Directory thumbnails: %1 ok, %2 failed").arg(ok).arg(fail));
          });
  dir_thumb_thread_->start();
}

void MainWindow::setup_toolbar()
{
  auto* toolbar = addToolBar(QStringLiteral("Main"));
  toolbar->setObjectName(QStringLiteral("mainToolBar"));
  toolbar->setMovable(false);
  toolbar->setIconSize(QSize(24, 24));
  toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

  // Order mirrors dirtoo-py FileViewWindow.make_toolbar:
  // Parent | Home | Back Forward | Reload Prepare | Show Hidden | Sort Group |
  // Icons SmallIcons Detail | Zoom± | LOD± | Crop
  parent_act_ = toolbar->addAction(theme_icon("go-up", "arrow-up"), QStringLiteral("Parent"), this,
                                 &MainWindow::on_go_parent);
  // Middle-click Parent → open parent in a new window.
  if (auto* btn = qobject_cast<QToolButton*>(toolbar->widgetForAction(parent_act_))) {
  btn->installEventFilter(this);
  }
  toolbar->addSeparator();
  toolbar->addAction(theme_icon("go-home", "user-home"), QStringLiteral("Home"), this, &MainWindow::on_go_home);
  toolbar->addSeparator();
  back_act_ = toolbar->addAction(theme_icon("go-previous", "arrow-left"), QStringLiteral("Back"), this,
                               &MainWindow::on_go_back);
  forward_act_ = toolbar->addAction(theme_icon("go-next", "arrow-right"), QStringLiteral("Forward"), this,
                                  &MainWindow::on_go_forward);
  toolbar->addSeparator();
  toolbar->addAction(theme_icon("view-refresh", "reload"), QStringLiteral("Reload"), this, &MainWindow::on_refresh);
  toolbar->addAction(theme_icon("image-x-generic"), QStringLiteral("Prepare Thumbnails"), this,
                   &MainWindow::on_prepare_thumbnails);
  toolbar->addSeparator();

  show_hidden_act_ = toolbar->addAction(theme_icon("view-hidden", "view-filter"),
                                      QStringLiteral("Show Hidden Files"));
  show_hidden_act_->setCheckable(true);
  show_hidden_act_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+H")));
  connect(show_hidden_act_, &QAction::toggled, this, &MainWindow::on_toggle_hidden);

  show_sidebar_act_ = toolbar->addAction(theme_icon("view-sidetree", "view-list-tree"),
                                       QStringLiteral("Directory Tree"));
  show_sidebar_act_->setCheckable(true);
  show_sidebar_act_->setChecked(true);
  show_sidebar_act_->setToolTip(QStringLiteral("Show or hide the directory tree sidebar"));
  show_sidebar_act_->setShortcut(QKeySequence(Qt::Key_F9));
  connect(show_sidebar_act_, &QAction::toggled, this, &MainWindow::on_toggle_sidebar);
  toolbar->addSeparator();

  // Sort / Group popup buttons — show current choice like a combo box.
  {
  sort_toolbar_btn_ = new QToolButton(toolbar);
  auto* sort_btn = sort_toolbar_btn_;
  sort_btn->setIcon(theme_icon("view-sort-ascending", "view-sort"));
  sort_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  sort_btn->setText(QStringLiteral("Name"));
  sort_btn->setToolTip(QStringLiteral("Sort"));
  sort_btn->setPopupMode(QToolButton::InstantPopup);
  auto* sort_menu = new QMenu(sort_btn);
  auto* sort_group = new QActionGroup(sort_btn);
  sort_group->setExclusive(true);
  auto add_sort = [&](const QString& title, dirtoo::collection::SortKey key, bool checked) {
    auto* act = sort_menu->addAction(title);
    act->setCheckable(true);
    act->setChecked(checked);
    sort_group->addAction(act);
    connect(act, &QAction::triggered, this, [this, key] {
      apply_sort_key(key, /*toggle_if_same=*/false);
    });
  };
  add_sort(QStringLiteral("Name"), dirtoo::collection::SortKey::Name, true);
  add_sort(QStringLiteral("Size"), dirtoo::collection::SortKey::Size, false);
  add_sort(QStringLiteral("Modified"), dirtoo::collection::SortKey::Modified, false);
  add_sort(QStringLiteral("Type"), dirtoo::collection::SortKey::Type, false);
  sort_menu->addSeparator();
  add_sort(QStringLiteral("Width"), dirtoo::collection::SortKey::Width, false);
  add_sort(QStringLiteral("Height"), dirtoo::collection::SortKey::Height, false);
  add_sort(QStringLiteral("Dimensions"), dirtoo::collection::SortKey::Resolution, false);
  add_sort(QStringLiteral("Aspect"), dirtoo::collection::SortKey::AspectRatio, false);
  add_sort(QStringLiteral("Duration"), dirtoo::collection::SortKey::Duration, false);
  add_sort(QStringLiteral("FPS"), dirtoo::collection::SortKey::Framerate, false);
  sort_menu->addSeparator();
  auto* asc = sort_menu->addAction(QStringLiteral("Ascending"));
  asc->setCheckable(true);
  asc->setChecked(true);
  connect(asc, &QAction::triggered, this, [this, asc] {
    sort_ascending_ = true;
    collection_.sorter().set_ascending(true);
    asc->setChecked(true);
    if (tree_view_ != nullptr && tree_view_->header() != nullptr) {
      tree_view_->header()->setSortIndicator(
          tree_view_->header()->sortIndicatorSection(), Qt::AscendingOrder);
    }
    request_async_sort();
  });
  auto* desc = sort_menu->addAction(QStringLiteral("Descending"));
  desc->setCheckable(true);
  connect(desc, &QAction::triggered, this, [this, desc, asc] {
    sort_ascending_ = false;
    collection_.sorter().set_ascending(false);
    desc->setChecked(true);
    asc->setChecked(false);
    if (tree_view_ != nullptr && tree_view_->header() != nullptr) {
      tree_view_->header()->setSortIndicator(
          tree_view_->header()->sortIndicatorSection(), Qt::DescendingOrder);
    }
    request_async_sort();
  });
  sort_btn->setMenu(sort_menu);
  toolbar->addWidget(sort_btn);
  }
  {
  group_toolbar_btn_ = new QToolButton(toolbar);
  auto* group_btn = group_toolbar_btn_;
  group_btn->setIcon(theme_icon("view-list-tree", "view-list"));
  group_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  group_btn->setText(QStringLiteral("None"));
  group_btn->setToolTip(QStringLiteral("Group by"));
  group_btn->setPopupMode(QToolButton::InstantPopup);
  auto* group_menu = new QMenu(group_btn);
  auto* group_actions = new QActionGroup(group_btn);
  group_actions->setExclusive(true);
  auto add_group = [&](const QString& title, dirtoo::collection::GroupMode mode, bool checked) {
    auto* act = group_menu->addAction(title);
    act->setCheckable(true);
    act->setChecked(checked);
    group_actions->addAction(act);
    connect(act, &QAction::triggered, this, [this, group_btn, title, mode] {
      group_btn->setText(title);
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
  group_btn->setMenu(group_menu);
  toolbar->addWidget(group_btn);
  }

  toolbar->addSeparator();
  // View modes: Icons, List (Win95-style), Detail
  icons_act_ = toolbar->addAction(theme_icon("view-grid", "view-list-icons"), QStringLiteral("Icons"));
  small_icons_act_ = toolbar->addAction(theme_icon("view-list", "view-list-details"),
                                      QStringLiteral("List"));
  detail_act_ = toolbar->addAction(theme_icon("view-list-details", "view-list"), QStringLiteral("Detail"));
  detail_act_->setCheckable(true);
  icons_act_->setCheckable(true);
  small_icons_act_->setCheckable(true);
  auto* view_group = new QActionGroup(this);
  view_group->addAction(icons_act_);
  view_group->addAction(small_icons_act_);
  view_group->addAction(detail_act_);
  connect(detail_act_, &QAction::triggered, this, &MainWindow::on_view_detail);
  connect(icons_act_, &QAction::triggered, this, &MainWindow::on_view_icons);
  connect(small_icons_act_, &QAction::triggered, this, &MainWindow::on_view_small_icons);
  detail_act_->setChecked(true);

  toolbar->addSeparator();
  toolbar->addAction(theme_icon("zoom-in"), QStringLiteral("Zoom +"), this, &MainWindow::on_zoom_in);
  toolbar->addAction(theme_icon("zoom-out"), QStringLiteral("Zoom −"), this, &MainWindow::on_zoom_out);
  toolbar->addSeparator();
  toolbar->addAction(theme_icon("zoom-fit-best", "list-add"), QStringLiteral("More detail"), this,
                   &MainWindow::on_more_icon_details);
  toolbar->addAction(theme_icon("zoom-original", "list-remove"), QStringLiteral("Less detail"), this,
                   &MainWindow::on_less_icon_details);
  toolbar->addSeparator();
  {
  auto* act = toolbar->addAction(theme_icon("crop-thumbnails", "zoom-fit-best"), QStringLiteral("Crop Thumbnails"));
  act->setCheckable(true);
  act->setToolTip(QStringLiteral("Crop thumbnails to fill the icon (cover) instead of letterboxing"));
  connect(act, &QAction::toggled, this, [this](bool on) {
    if (model_ != nullptr) {
      model_->set_crop_thumbnails(on);
    }
    if (icon_view_ != nullptr) {
      icon_view_->viewport()->update();
    }
    if (tree_view_ != nullptr) {
      tree_view_->viewport()->update();
    }
    if (graphics_view_ != nullptr) {
      graphics_view_->viewport()->update();
    }
  });
  crop_thumbnails_act_ = act;
  }

}

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
        {"type", "Type"},
    };
    for (const auto& c : kCols) {
      auto* act = cols->addAction(QString::fromUtf8(c.label));
      act->setCheckable(true);
      const QString key = QString::fromUtf8(c.key);
      act->setChecked(detail_columns_.contains(key)
                      || (detail_columns_.isEmpty()
                          && key != QLatin1String("width") && key != QLatin1String("height")));
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

void MainWindow::setup_shortcuts()
{
  // Only shortcuts not already bound on menu/toolbar actions (those are
  // ambiguous if registered again via addAction).
  auto add_shortcut = [this](const QKeySequence& seq, auto slot) {
    auto* act = new QAction(this);
    act->setShortcut(seq);
    connect(act, &QAction::triggered, this, slot);
    addAction(act);
    return act;
  };
  add_shortcut(QKeySequence(Qt::Key_Backspace), &MainWindow::on_go_parent);
  add_shortcut(QKeySequence(Qt::ALT | Qt::Key_Up), &MainWindow::on_go_parent);
  add_shortcut(QKeySequence(Qt::ALT | Qt::Key_Home), &MainWindow::on_go_home);
  add_shortcut(QKeySequence(Qt::ALT | Qt::Key_Left), &MainWindow::on_go_back);
  add_shortcut(QKeySequence(Qt::ALT | Qt::Key_Right), &MainWindow::on_go_forward);
  add_shortcut(QKeySequence(QStringLiteral("Ctrl+L")), &MainWindow::on_focus_location);
  add_shortcut(QKeySequence(Qt::Key_Escape), &MainWindow::on_clear_filter);
  add_shortcut(QKeySequence(QStringLiteral("Ctrl+D")), &MainWindow::on_toggle_bookmark);
}

void MainWindow::setup_central_ui()
{
  auto* central = new QWidget(this);
  auto* central_layout = new QHBoxLayout(central);
  central_layout->setContentsMargins(0, 0, 0, 0);
  central_layout->setSpacing(0);

  main_splitter_ = new QSplitter(Qt::Horizontal, central);
  main_splitter_->setChildrenCollapsible(false);

  // Left: directory tree sidebar
  sidebar_widget_ = new QWidget(main_splitter_);
  auto* sidebar_layout = new QVBoxLayout(sidebar_widget_);
  sidebar_layout->setContentsMargins(0, 0, 0, 0);
  sidebar_layout->setSpacing(0);

  // Devices panel (top of vertical sidebar splitter).
  auto* devices_panel = new QWidget(sidebar_widget_);
  auto* devices_layout = new QVBoxLayout(devices_panel);
  devices_layout->setContentsMargins(0, 0, 0, 0);
  devices_layout->setSpacing(0);
  devices_label_ = new QLabel(QStringLiteral("Devices"), devices_panel);
  devices_label_->setStyleSheet(QStringLiteral("font-weight: bold; padding: 4px 6px 2px 6px;"));
  devices_layout->addWidget(devices_label_);
  devices_list_ = new QListWidget(devices_panel);
  devices_list_->setFrameShape(QFrame::NoFrame);
  devices_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  devices_list_->setContextMenuPolicy(Qt::CustomContextMenu);
  // Devices list wired in DevicesController setup (after udisks client creation).
  devices_layout->addWidget(devices_list_, 1);

  // Places + directory tree (bottom of vertical sidebar splitter).
  auto* places_panel = new QWidget(sidebar_widget_);
  auto* places_layout = new QVBoxLayout(places_panel);
  places_layout->setContentsMargins(0, 0, 0, 0);
  places_layout->setSpacing(0);
  auto* places_label = new QLabel(QStringLiteral("Places"), places_panel);
  places_label->setStyleSheet(QStringLiteral("font-weight: bold; padding: 6px 6px 2px 6px;"));
  places_layout->addWidget(places_label);

  directory_tree_model_ = new DirectoryTreeModel(this);
  rebuild_sidebar_places();
  devices_controller_ = new DevicesController(this);
  devices_controller_->set_list_widget(devices_list_);
  devices_controller_->set_parent_widget(this);
  connect(devices_list_, &QListWidget::itemActivated, devices_controller_,
        &DevicesController::on_item_activated);
  connect(devices_list_, &QListWidget::itemClicked, devices_controller_,
        &DevicesController::on_item_activated);
  connect(devices_list_, &QWidget::customContextMenuRequested, devices_controller_,
        &DevicesController::on_context_menu);
  connect(devices_controller_, &DevicesController::open_path, this, [this](const QString& path) {
  open_location(fs::Location::from_path(std::filesystem::path(path.toStdString())), true);
  });
  connect(devices_controller_, &DevicesController::status_message, this, &MainWindow::set_status);
  devices_controller_->refresh();

  sidebar_tree_ = new QTreeView(places_panel);
  sidebar_tree_->setModel(directory_tree_model_);
  sidebar_tree_->setHeaderHidden(true);
  sidebar_tree_->setUniformRowHeights(true);
  sidebar_tree_->setAnimated(true);
  sidebar_tree_->setExpandsOnDoubleClick(true);
  sidebar_tree_->setFrameShape(QFrame::NoFrame);
  sidebar_tree_->setSelectionMode(QAbstractItemView::SingleSelection);
  connect(sidebar_tree_, &QTreeView::activated, this, &MainWindow::on_sidebar_activated);
  connect(sidebar_tree_, &QTreeView::clicked, this, &MainWindow::on_sidebar_activated);
  places_layout->addWidget(sidebar_tree_, 1);

  auto* sidebar_splitter = new QSplitter(Qt::Vertical, sidebar_widget_);
  sidebar_splitter->setChildrenCollapsible(false);
  sidebar_splitter->addWidget(devices_panel);
  sidebar_splitter->addWidget(places_panel);
  sidebar_splitter->setStretchFactor(0, 0);
  sidebar_splitter->setStretchFactor(1, 1);
  sidebar_splitter->setSizes({140, 400});
  sidebar_layout->addWidget(sidebar_splitter);

  main_splitter_->addWidget(sidebar_widget_);

  // Right: existing chrome + file views
  auto* right = new QWidget(main_splitter_);
  auto* layout = new QVBoxLayout(right);
  // Flush to window edges so the view scrollbar sits on the window border
  // (dirtoo-py form margins are 0).
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  {
  auto* loc_host = new QWidget(central);
  auto* loc_layout = new QVBoxLayout(loc_host);
  loc_layout->setContentsMargins(0, 0, 0, 0);
  loc_layout->setSpacing(0);

  auto* breadcrumb_row = new QWidget(loc_host);
  auto* breadcrumb_layout = new QHBoxLayout(breadcrumb_row);
  breadcrumb_layout->setContentsMargins(4, 2, 4, 2);
  breadcrumb_layout->setSpacing(6);
  auto* loc_label = new QLabel(QStringLiteral("Location:"), breadcrumb_row);
  loc_label->setStyleSheet(QStringLiteral("font-weight: bold;"));
  breadcrumb_layout->addWidget(loc_label);

  location_buttons_ = new LocationButtonBar(breadcrumb_row);
  breadcrumb_layout->addWidget(location_buttons_, 1);
  connect(location_buttons_, &LocationButtonBar::location_activated, this,
          &MainWindow::on_breadcrumb_location);
  connect(location_buttons_, &LocationButtonBar::location_activated_new_window, this,
          &MainWindow::on_breadcrumb_location_new_window);
  connect(location_buttons_, &LocationButtonBar::edit_requested, this,
          &MainWindow::on_location_edit_requested);
  connect(location_buttons_, &LocationButtonBar::urls_dropped, this,
          &MainWindow::on_breadcrumb_drop);

  location_edit_ = new QLineEdit(breadcrumb_row);
  location_edit_->setPlaceholderText(QStringLiteral("Location"));
  {
    path_completion_model_ = new QStringListModel(location_edit_);
    path_completer_ = new QCompleter(path_completion_model_, location_edit_);
    path_completer_->setCaseSensitivity(Qt::CaseInsensitive);
    path_completer_->setCompletionMode(QCompleter::PopupCompletion);
    path_completer_->setFilterMode(Qt::MatchStartsWith);
    location_edit_->setCompleter(path_completer_);

    path_completion_timer_ = new QTimer(this);
    path_completion_timer_->setSingleShot(true);
    path_completion_timer_->setInterval(60);
    connect(path_completion_timer_, &QTimer::timeout, this,
            &MainWindow::on_path_completion_timeout);

    path_completion_thread_ = new QThread(this);
    path_completion_worker_ = new PathCompletionWorker();
    path_completion_worker_->moveToThread(path_completion_thread_);
    connect(path_completion_thread_, &QThread::finished, path_completion_worker_,
            &QObject::deleteLater);
    connect(path_completion_worker_, &PathCompletionWorker::completions_ready, this,
            &MainWindow::on_path_completions_ready);
    path_completion_thread_->start();

    connect(location_edit_, &QLineEdit::textEdited, this, &MainWindow::on_location_text_edited);
  }

  connect(location_edit_, &QLineEdit::returnPressed, this, &MainWindow::on_location_entered);
  connect(location_edit_, &QLineEdit::editingFinished, this, [this] {
    // Leave edit mode when focus leaves, unless empty.
    if (!location_edit_->hasFocus()) {
      show_location_buttons();
    }
  });
  location_edit_->hide();
  breadcrumb_layout->addWidget(location_edit_, 1);

  loc_layout->addWidget(breadcrumb_row);
  layout->addWidget(loc_host);
  location_stack_host_ = loc_host;
  }

  // Search row: same expression language + Help.
  {
  auto* search_row = new QWidget(central);
  auto* search_layout = new QHBoxLayout(search_row);
  search_layout->setContentsMargins(6, 2, 6, 2);
  search_layout->setSpacing(6);
  auto* search_label = new QLabel(QStringLiteral("Search:"), search_row);
  search_edit_ = new QLineEdit(search_row);
  search_edit_->setPlaceholderText(
      QStringLiteral("Recursive search (filter expression, Enter to run, Esc to close)…"));
  search_edit_->setVisible(true);
  search_edit_->installEventFilter(this);
  connect(search_edit_, &QLineEdit::returnPressed, this, &MainWindow::on_search_submitted);
  search_label->setBuddy(search_edit_);
  auto* search_help_btn = new QPushButton(QStringLiteral("Help"), search_row);
  search_help_btn->setToolTip(QStringLiteral("Filter expression language help"));
  connect(search_help_btn, &QPushButton::clicked, this, &MainWindow::on_show_filter_help);
  search_layout->addWidget(search_label);
  search_layout->addWidget(search_edit_, 1);
  search_layout->addWidget(search_help_btn);
  search_row->setVisible(false);
  search_row_ = search_row;
  layout->addWidget(search_row);
  }

  message_area_ = new MessageArea(central);
  layout->addWidget(message_area_);

  model_ = new FileListModel(this);
  model_->set_icon_detail_level(3);
  dirtoo::filter::MediaMetaCache::instance().open();
  model_->set_collection(&collection_);
  connect(model_, &FileListModel::urls_dropped, this, &MainWindow::on_urls_dropped_to);

  view_stack_ = new QStackedWidget(central);

  tree_view_ = new FileTreeView(view_stack_);
  tree_view_->setModel(model_);
  tree_view_->setFrameShape(QFrame::NoFrame);
  tree_view_->setRootIsDecorated(false);
  // Prefer uniform heights for large-dir scroll cost; toggle off when group/time-gap
  // separators need variable row height (see update_detail_row_heights).
  tree_view_->setUniformRowHeights(true);
  tree_view_->setAlternatingRowColors(true);
  tree_view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  tree_view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  tree_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  tree_view_->setSortingEnabled(false);
  tree_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  tree_view_->setDragEnabled(true);
  tree_view_->setAcceptDrops(true);
  tree_view_->viewport()->setAcceptDrops(true);
  tree_view_->setDropIndicatorShown(true);
  tree_view_->setDragDropMode(QAbstractItemView::DragDrop);
  tree_view_->setDefaultDropAction(Qt::CopyAction);
  tree_view_->setDragDropOverwriteMode(false);
  tree_view_->setIconSize(QSize(32, 32));
  tree_view_->header()->setStretchLastSection(true);
  tree_view_->header()->setSectionsClickable(true);
  tree_view_->header()->setSortIndicatorShown(true);
  tree_view_->setColumnWidth(0, 320);
  tree_view_->setColumnWidth(1, 100);
  tree_view_->setColumnWidth(2, 160);
  tree_view_->setItemDelegate(new FileItemDelegate(model_, tree_view_));
  connect(tree_view_, &QTreeView::activated, this, &MainWindow::on_item_activated);
  tree_view_->viewport()->installEventFilter(this);
  tree_view_->installEventFilter(this);
  connect(tree_view_, &QWidget::customContextMenuRequested, this, &MainWindow::on_context_menu);
  connect(tree_view_->header(), &QHeaderView::sectionClicked, this, &MainWindow::on_header_clicked);
  connect(tree_view_->verticalScrollBar(), &QScrollBar::valueChanged, this,
        [this](int) { request_thumbnails_for_visible(); });
  view_stack_->addWidget(tree_view_);

  icon_view_ = new FileListView(view_stack_);
  icon_view_->setModel(model_);
  icon_view_->setFrameShape(QFrame::NoFrame);
  icon_view_->setViewMode(QListView::IconMode);
  icon_view_->setResizeMode(QListView::Adjust);
  icon_view_->setMovement(QListView::Static);
  icon_view_->setUniformItemSizes(true);
  icon_view_->setItemDelegate(new FileItemDelegate(model_, icon_view_));
  icon_view_->setWordWrap(true);
  icon_view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  icon_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  icon_view_->setDragEnabled(true);
  icon_view_->setAcceptDrops(true);
  icon_view_->viewport()->setAcceptDrops(true);
  icon_view_->setDropIndicatorShown(true);
  icon_view_->setDragDropMode(QAbstractItemView::DragDrop);
  icon_view_->setDefaultDropAction(Qt::CopyAction);
  icon_view_->setDragDropOverwriteMode(false);
  connect(icon_view_, &QListView::activated, this, &MainWindow::on_item_activated);
  icon_view_->viewport()->installEventFilter(this);
  icon_view_->installEventFilter(this);
  connect(icon_view_, &QWidget::customContextMenuRequested, this, &MainWindow::on_context_menu);
  connect(icon_view_->verticalScrollBar(), &QScrollBar::valueChanged, this,
        [this](int) { request_thumbnails_for_visible(); });
  view_stack_->addWidget(icon_view_);

  graphics_view_ = new GraphicsFileView(view_stack_);
  graphics_view_->set_model(model_);
  graphics_view_->setFrameShape(QFrame::NoFrame);
  graphics_view_->viewport()->installEventFilter(this);
  graphics_view_->installEventFilter(this);
  connect(graphics_view_, &GraphicsFileView::activated, this, &MainWindow::on_item_activated);
  connect(graphics_view_, &GraphicsFileView::middle_clicked, this, &MainWindow::on_view_middle_click);
  connect(graphics_view_, &GraphicsFileView::context_menu_requested, this,
        [this](const QPoint& global_pos, const QModelIndex&) {
          on_context_menu(graphics_view_->mapFromGlobal(global_pos));
        });
  connect(graphics_view_, &GraphicsFileView::selection_changed, this,
        &MainWindow::on_selection_changed);
  connect(graphics_view_, &GraphicsFileView::files_dropped, this, &MainWindow::on_urls_dropped_to);
  connect(graphics_view_->verticalScrollBar(), &QScrollBar::valueChanged, this,
        [this](int) { request_thumbnails_for_visible(); });
  view_stack_->addWidget(graphics_view_);

  apply_icon_zoom();

  connect(tree_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
        &MainWindow::on_selection_changed);
  connect(icon_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
        &MainWindow::on_selection_changed);

  layout->addWidget(view_stack_, 1);

  // Filter row at bottom (parity with dirtoo-py BottomToolBarArea filter toolbar).
  {
  auto* filter_row = new QWidget(central);
  filter_row->setAutoFillBackground(true);
  filter_row->setBackgroundRole(QPalette::Window);
  auto* filter_layout = new QHBoxLayout(filter_row);
  filter_layout->setContentsMargins(6, 2, 6, 2);
  filter_layout->setSpacing(6);
  auto* filter_label = new QLabel(QStringLiteral("Filter:"), filter_row);
  filter_edit_ = new QLineEdit(filter_row);
  filter_edit_->setPlaceholderText(
      QStringLiteral("Filter by name, glob, or expression (e.g. *.png, size:>1M)…"));
  filter_edit_->setClearButtonEnabled(true);
  filter_edit_->setEnabled(true);
  filter_edit_->setVisible(true);
  filter_edit_->installEventFilter(this);
  connect(filter_edit_, &QLineEdit::textChanged, this, &MainWindow::on_filter_changed);
  filter_label->setBuddy(filter_edit_);
  auto* filter_help_btn = new QPushButton(QStringLiteral("Help"), filter_row);
  filter_help_btn->setToolTip(QStringLiteral("Filter expression language help"));
  filter_help_btn->setFlat(false);
  connect(filter_help_btn, &QPushButton::clicked, this, &MainWindow::on_show_filter_help);
  filter_layout->addWidget(filter_label);
  filter_layout->addWidget(filter_edit_, 1);
  filter_layout->addWidget(filter_help_btn);
  filter_row_ = filter_row;
  layout->addWidget(filter_row);
  }

  main_splitter_->addWidget(right);
  main_splitter_->setStretchFactor(0, 0);
  main_splitter_->setStretchFactor(1, 1);
  main_splitter_->setSizes({220, 800});
  central_layout->addWidget(main_splitter_);
  setCentralWidget(central);

}

void MainWindow::setup_status_and_services()
{
  // Real QStatusBar: left = filename / messages, right = size info (dirtoo-py).
  status_label_ = new QLabel(this);
  statusBar()->addWidget(status_label_, 1);
  status_info_label_ = new QLabel(this);
  status_info_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  statusBar()->addPermanentWidget(status_info_label_);
  statusBar()->setSizeGripEnabled(true);

  leap_widget_ = new LeapWidget(this);
  connect(leap_widget_, &LeapWidget::leap, this, &MainWindow::on_leap);

  // Debounce watcher storms (busy dirs otherwise clear+reload+rethumb every event).
  watcher_reload_timer_ = new QTimer(this);
  watcher_reload_timer_->setSingleShot(true);
  watcher_reload_timer_->setInterval(200);
  connect(watcher_reload_timer_, &QTimer::timeout, this, [this] { reload_directory(true); });
  connect(&watcher_, &watcher::DirectoryWatcher::directory_changed, this, [this] {
  if (watcher_reload_timer_ != nullptr) {
    watcher_reload_timer_->start();
  }
  });
  connect(&watcher_, &watcher::DirectoryWatcher::message, this, [this](const QString& msg) {
  set_status(msg);
  });

  connect(&thumbnailer_, &thumbnail::Thumbnailer::thumbnail_ready, this,
        &MainWindow::on_thumbnail_ready);
  connect(&thumbnailer_, &thumbnail::Thumbnailer::thumbnail_failed, this,
        &MainWindow::on_thumbnail_failed);

  connect(qApp->clipboard(), &QClipboard::dataChanged, this, &MainWindow::update_edit_actions);

  connect(&archive_manager_, &archive::ArchiveManager::extraction_started, this,
        [this](const fs::Location&) {
          set_status(QStringLiteral("Extracting archive…"));
        });
  connect(&archive_manager_, &archive::ArchiveManager::extraction_ready, this,
        &MainWindow::on_archive_ready);
  connect(&archive_manager_, &archive::ArchiveManager::extraction_failed, this,
        &MainWindow::on_archive_failed);

}


} // namespace dirtoo::app

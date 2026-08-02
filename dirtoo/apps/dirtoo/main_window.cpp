// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"
#include "badge_icons.hpp"
#include "theme_icons.hpp"
#include "location_icons.hpp"
#include "location_url.hpp"
#include "location_menu_helpers.hpp"
#include "directory_tree_model.hpp"
#include "udisks_client.hpp"
#include "devices_controller.hpp"
#include "filter_worker.hpp"
#include "directory_thumbnail_worker.hpp"
#include "file_views.hpp"
#include "file_context_menu.hpp"
#include "file_list_model.hpp"
#include "graphics_file_view.hpp"
#include "graphics_file_item.hpp"
#include "file_item_delegate.hpp"
#include "dirtoo/filter/media_meta_cache.hpp"

#include "dirtoo/filter/parser.hpp"

#include "clipboard.hpp"
#include "about_dialog.hpp"
#include "name_input_dialog.hpp"
#include "app_settings.hpp"
#include "size_format.hpp"
#include "conflict_dialog.hpp"
#include "open_with.hpp"
#include "open_history.hpp"
#include "operations_history.hpp"
#include "preferences_dialog.hpp"
#include "properties_dialog.hpp"
#include "archive_member_cache.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/thumbnail/thumbnailer.hpp"
#include "dirops/ops.hpp"
#include "dirops/util.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QtConcurrent>
#include "dirtoo/archive/archive_index.hpp"
#include <QApplication>
#include <QDateTime>
#include <QCoreApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QShowEvent>
#include <QCompleter>
#include <QStringListModel>
#include <QTimer>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QFrame>
#include <QFileDialog>
#include <QTextStream>
#include <QMouseEvent>
#include <QDesktopServices>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <set>
#include <QHeaderView>
#include <QHash>
#include <QIcon>
#include <QPainter>
#include <QSvgRenderer>
#include <QKeyEvent>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMimeDatabase>
#include <QPixmap>
#include <QProcess>
#include <QSplitter>
#include <QStyle>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QScrollBar>
#include <QUrl>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QBrush>
#include <QColor>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <algorithm>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <filesystem>
#include <optional>

namespace dirtoo::app {


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
  setWindowTitle(QStringLiteral("dirtoo"));
  resize(960, 640);

  setup_background_workers();
  setup_toolbar();
  setup_menus();
  setup_shortcuts();
  setup_central_ui();
  setup_status_and_services();

  update_history_actions();
  update_edit_actions();
  set_view_mode(ViewMode::Detail);
  restore_settings();
}

MainWindow::~MainWindow()
{
  stop_search();
  if (path_completion_worker_ != nullptr) {
    path_completion_worker_->cancel();
  }
  if (path_completion_thread_ != nullptr) {
    path_completion_thread_->quit();
    path_completion_thread_->wait(2000);
  }
  transfer_controller_.shutdown();
  search_controller_.stop();
  if (dir_load_thread_ != nullptr) {
    dir_load_thread_->quit();
    dir_load_thread_->wait(3000);
  }
  if (sort_thread_ != nullptr) {
    sort_thread_->quit();
    sort_thread_->wait(3000);
  }
  if (filter_thread_ != nullptr) {
    filter_thread_->quit();
    filter_thread_->wait(3000);
  }
  if (dir_thumb_thread_ != nullptr) {
    dir_thumb_thread_->quit();
    dir_thumb_thread_->wait(3000);
  }
}

QAbstractItemView* MainWindow::current_view() const
{
  if (view_mode_ == ViewMode::Detail) {
    return tree_view_;
  }
  return icon_view_;
}


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

void MainWindow::apply_icon_zoom()
{
  if (view_mode_ == ViewMode::SmallIcons) {
    // Windows 95 Explorer "List" view: small icon left of filename, columns
    // filled top-to-bottom then left-to-right.
    static constexpr int kSmall[] = {16, 24, 32, 48, 64, 96, 128};
    const int zi = std::clamp(zoom_list_, 0, static_cast<int>(std::size(kSmall)) - 1);
    const int size = kSmall[zi];
    icon_view_->setViewMode(QListView::ListMode);
    icon_view_->setFlow(QListView::TopToBottom);
    icon_view_->setWrapping(true);
    icon_view_->setResizeMode(QListView::Adjust);
    icon_view_->setMovement(QListView::Static);
    icon_view_->setUniformItemSizes(true);
    icon_view_->setWordWrap(false);
    icon_view_->setIconSize(QSize(size, size));
    icon_view_->setSpacing(2);
    // Row a little taller than the icon/filename; column wide enough for a name.
    const int row_h = std::max(size, 16) + 6;
    const int col_w = std::max(size + 12 + 140, 180);
    icon_view_->setGridSize(QSize(col_w, row_h));
    if (model_ != nullptr) {
      // List layout: Decoration left, DisplayRole text to the right (not under icon).
      model_->set_icon_style(false);
      model_->set_icon_detail_level(1);
    }
    return;
  }

  if (view_mode_ == ViewMode::Detail) {
    static constexpr int kDetail[] = {16, 24, 32, 48, 64, 96, 128};
    const int zi = std::clamp(zoom_detail_, 0, static_cast<int>(std::size(kDetail)) - 1);
    const int size = kDetail[zi];
    if (tree_view_ != nullptr) {
      tree_view_->setIconSize(QSize(size, size));
    }
    return;
  }

  const int size = kZoomLevels[std::clamp(zoom_icons_, 0, static_cast<int>(std::size(kZoomLevels)) - 1)];
  if (view_mode_ == ViewMode::Icons) {
    icon_view_->setViewMode(QListView::IconMode);
    icon_view_->setFlow(QListView::LeftToRight);
    icon_view_->setWrapping(true);
    icon_view_->setResizeMode(QListView::Adjust);
    icon_view_->setUniformItemSizes(true);
  }
  icon_view_->setIconSize(QSize(size, size));
  const int text_rows = model_ != nullptr ? model_->icon_text_rows() : 1;
  const int text_h = 6 + text_rows * 18;
  const int cell_w = std::max(size + 40, 96);
  const int cell_h = size + text_h + 16;
  icon_view_->setGridSize(QSize(cell_w, cell_h));
  icon_view_->setSpacing(8);
  if (graphics_view_ != nullptr) {
    graphics_view_->set_tile_size(QSize(cell_w, cell_h));
    graphics_view_->set_compact(false);
    graphics_view_->relayout();
  }
  const int detail = std::max(16, size / 4);
  tree_view_->setIconSize(QSize(detail, detail));
}

void MainWindow::apply_icon_detail_level()
{
  if (model_ == nullptr) {
    return;
  }
  if (status_label_ != nullptr && view_mode_ == ViewMode::Icons) {
    static const char* labels[] = {
        "Icon captions: none",
        "Icon captions: name",
        "Icon captions: name",
        "Icon captions: name + size",
        "Icon captions: name + size + date",
    };
    const int lvl = model_->icon_detail_level();
    set_status(QString::fromUtf8(labels[std::clamp(lvl, 0, 4)]));
  }
}

void MainWindow::on_more_icon_details()
{
  if (model_ == nullptr) {
    return;
  }
  model_->set_icon_detail_level(model_->icon_detail_level() + 1);
  apply_icon_zoom();
  apply_icon_detail_level();
}

void MainWindow::on_less_icon_details()
{
  if (model_ == nullptr) {
    return;
  }
  model_->set_icon_detail_level(model_->icon_detail_level() - 1);
  apply_icon_zoom();
  apply_icon_detail_level();
}

int& MainWindow::zoom_for_current_view()
{
  switch (view_mode_) {
  case ViewMode::SmallIcons:
    return zoom_list_;
  case ViewMode::Detail:
    return zoom_detail_;
  case ViewMode::Icons:
  default:
    return zoom_icons_;
  }
}

int MainWindow::zoom_for_current_view() const
{
  switch (view_mode_) {
  case ViewMode::SmallIcons:
    return zoom_list_;
  case ViewMode::Detail:
    return zoom_detail_;
  case ViewMode::Icons:
  default:
    return zoom_icons_;
  }
}

void MainWindow::on_zoom_in()
{
  const int max_zi = (view_mode_ == ViewMode::Icons)
                         ? static_cast<int>(std::size(kZoomLevels)) - 1
                         : 6;
  int& zi = zoom_for_current_view();
  if (zi < max_zi) {
    ++zi;
    apply_icon_zoom();
  }
}

void MainWindow::on_zoom_out()
{
  int& zi = zoom_for_current_view();
  if (zi > 0) {
    --zi;
    apply_icon_zoom();
  }
}


void MainWindow::set_view_mode(ViewMode mode)
{
  view_mode_ = mode;
  if (mode == ViewMode::Detail) {
    if (model_ != nullptr) {
      model_->set_icon_style(false);
    }
    view_stack_->setCurrentWidget(tree_view_);
    if (detail_act_ != nullptr) {
      detail_act_->setChecked(true);
    }
    apply_icon_zoom();
    apply_detail_column_visibility();
    request_thumbnails_for_visible();
  } else if (mode == ViewMode::SmallIcons) {
    if (model_ != nullptr) {
      model_->set_icon_style(false);
      model_->set_icon_detail_level(1);
    }
    view_stack_->setCurrentWidget(icon_view_);
    if (small_icons_act_ != nullptr) {
      small_icons_act_->setChecked(true);
    }
    apply_icon_zoom();
    request_thumbnails_for_visible();
  } else {
    if (model_ != nullptr) {
      model_->set_icon_style(true);
      // List view forces detail level 1; restore a useful multi-line caption LOD for Icons.
      if (model_->icon_detail_level() <= 1) {
        model_->set_icon_detail_level(3);
      }
    }
    if (graphics_view_ != nullptr) {
      view_stack_->setCurrentWidget(graphics_view_);
      graphics_view_->sync_from_model();
    } else {
      view_stack_->setCurrentWidget(icon_view_);
    }
    if (icons_act_ != nullptr) {
      icons_act_->setChecked(true);
    }
    apply_icon_zoom();
    request_thumbnails_for_visible();
  }
}

void MainWindow::on_view_detail()
{
  set_view_mode(ViewMode::Detail);
}

void MainWindow::on_view_icons()
{
  set_view_mode(ViewMode::Icons);
}

void MainWindow::on_view_small_icons()
{
  set_view_mode(ViewMode::SmallIcons);
}

void MainWindow::set_status(const QString& text)
{
  if (status_label_ != nullptr) {
    status_label_->setText(text);
  }
  if (!text.isEmpty()) {
    qInfo().noquote() << QStringLiteral("status: %1").arg(text);
  }
}

void MainWindow::open_location(const fs::Location& location, bool record_history)
{

  qInfo().noquote() << QStringLiteral("open_location %1 (history=%2)")
                           .arg(QString::fromStdString(location.as_url()))
                           .arg(record_history);
  stop_search();
  search_active_ = false;
  search_results_.clear();
  if (search_row_ != nullptr ? search_row_->isVisible()
      : (search_edit_ != nullptr && search_edit_->isVisible())) {
    if (search_row_ != nullptr) {
      search_row_->hide();
    } else if (search_edit_ != nullptr) {
      search_edit_->hide();
    }
  }
  // Reset filter on directory change unless Pin Filter is active.
  if (!filter_pinned_ && filter_edit_ != nullptr && !filter_edit_->text().isEmpty()) {
    filter_edit_->blockSignals(true);
    filter_edit_->clear();
    filter_edit_->blockSignals(false);
    collection_.set_name_filter(std::string{});
    update_filter_chrome(false);
  }
  location_ = location;
  if (location_.is_archive()) {
    location_edit_->setText(QString::fromStdString(location_.as_url()));
  } else {
    location_edit_->setText(QString::fromStdString(location_.as_path().string()));
  }
  if (location_buttons_ != nullptr) {
    location_buttons_->set_location(location_);
  }
  show_location_buttons();

  nav_history_.push(location, record_history);
  update_history_actions();

  if (location_.is_archive()) {
    pending_archive_location_ = location_;

    // Reuse index when navigating within the same archive file.
    if (archive_listing_.ready_for(location_.as_path())) {
      start_watcher_for_location();
      on_directory_changed();
    } else {
      QApplication::setOverrideCursor(Qt::WaitCursor);
      std::string list_err;
      const bool listed = archive_listing_.load(location_.as_path(), &list_err);
      QApplication::restoreOverrideCursor();
      if (listed) {
        set_status(QStringLiteral("Archive index: %1 entries")
                                   .arg(archive_listing_.entries().size()));
        start_watcher_for_location();
        on_directory_changed();
      } else {
        set_status(QStringLiteral("Listing failed (%1); extracting…")
                                   .arg(QString::fromStdString(list_err)));
        if (archive_manager_.status(fs::Location::from_archive(location_.as_path(), {}))
            != archive::ExtractStatus::Ready) {
          QApplication::setOverrideCursor(Qt::WaitCursor);
        }
        archive_manager_.open(location_);
      }
    }
  } else {
    start_watcher_for_location();
    // Initial listing (watcher no longer emits on start).
    reload_directory(false);
  }

  if (auto* view = current_view()) {
    view->setFocus(Qt::OtherFocusReason);
  }
  sync_sidebar_to_location();

}

void MainWindow::on_location_entered()
{
  try {
    open_location(fs::Location::from_human(location_edit_->text().toStdString()));
  } catch (const std::exception& ex) {
    set_status(QString::fromUtf8(ex.what()));
  }
}

void MainWindow::on_go_parent()
{
  open_location(location_.parent());
}

void MainWindow::on_go_home()
{
  open_location(fs::Location::from_path(std::filesystem::path{QDir::homePath().toStdString()}));
}

void MainWindow::on_go_back()
{
  if (const auto loc = nav_history_.go_back()) {
    open_location(*loc, false);
  }
}

void MainWindow::on_go_forward()
{
  if (const auto loc = nav_history_.go_forward()) {
    open_location(*loc, false);
  }
}

void MainWindow::update_history_actions()
{
  if (back_act_) {
    back_act_->setEnabled(nav_history_.can_go_back());
  }
  if (forward_act_) {
    forward_act_->setEnabled(nav_history_.can_go_forward());
  }
}


void MainWindow::update_edit_actions()
{
  if (paste_act_) {
    paste_act_->setEnabled(!transfer_controller_.busy()
                           && clipboard_has_paths(QApplication::clipboard()->mimeData()));
  }
}

void MainWindow::on_directory_changed()
{
  reload_directory(false);
}

void MainWindow::reload_directory(bool soft)
{
  if (search_active_) {
    // Keep recursive search results until the user navigates away or closes search.
    return;
  }
  qInfo().noquote() << QStringLiteral("reload_directory soft=%1 path=%2")
                           .arg(soft)
                           .arg(QString::fromStdString(location_.as_url()));
  soft_directory_reload_ = soft;
  if (!soft) {
    // Navigation/explicit refresh supersedes any pending soft watcher tick.
    if (watcher_reload_timer_ != nullptr) {
      watcher_reload_timer_->stop();
    }
    thumbnailer_.cancel_all();
    thumb_alias_.clear();
    if (model_ != nullptr) {
      model_->clear_thumbnails();
    }
    filter::MediaMetaCache::instance().bump_generation();
  }

  // In-memory archive index: apply on UI thread (no directory walk).
  // Soft watcher ticks refresh the TOC only when the archive file stamp changes.
  if (location_.is_archive()) {
    std::string list_err;
    (void)archive_listing_.refresh_if_stale(location_.as_path(), &list_err);
  }
  if (location_.is_archive() && archive_listing_.ok()) {
    soft_directory_reload_ = false;
    auto items = archive_listing_.fileinfos_for(location_);
    if (model_ != nullptr) {
      model_->clear_child_counts();
      for (const auto& [path, n] : archive_listing_.child_counts_for(location_)) {
        model_->set_child_count(QString::fromStdString(path), static_cast<qint64>(n));
      }
    }
    collection_.sorter().set_ascending(sort_ascending_);
    collection_.set_items(std::move(items));
    if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()) {
      if (filter_expression_needs_content_io(filter_edit_->text())) {
        request_async_filter();
      } else {
        collection_.set_name_filter(filter_edit_->text().toStdString());
      }
    }
    refresh_list();
    request_thumbnails_for_visible();
    return;
  }

  fs::Location load_loc = location_;
  if (location_.is_archive()) {
    const auto resolved = archive_manager_.resolved_directory(location_);
    if (!resolved) {
      set_status(QStringLiteral("Archive not ready"));
      return;
    }
    load_loc = fs::Location::from_path(*resolved);
  }

  const quint64 gen = ++dir_load_generation_;
  set_status(soft ? QStringLiteral("Refreshing…") : QStringLiteral("Loading…"));
  // Hard navigation: clear immediately so the old directory does not linger.
  // Soft (watcher): keep showing the previous listing until the worker finishes.
  if (!soft) {
    collection_.clear();
    refresh_list();
  }

  if (dir_load_worker_ == nullptr) {
    return;
  }
  // Supersede any in-flight listing (especially important when soft refreshes stack up).
  QMetaObject::invokeMethod(dir_load_worker_, "cancel", Qt::QueuedConnection);
  const QString path = QString::fromStdString(load_loc.as_path().string());
  QMetaObject::invokeMethod(dir_load_worker_, "load", Qt::QueuedConnection,
                            Q_ARG(QString, path), Q_ARG(quint64, gen));
}

void MainWindow::on_directory_loaded(quint64 generation, std::vector<fs::FileInfo> items)
{
  qInfo().noquote() << QStringLiteral("directory_loaded gen=%1 items=%2")
                           .arg(generation)
                           .arg(items.size());

  if (generation != dir_load_generation_ || search_active_) {
    return;
  }
  // "New" badge: paths that appeared since we last listed this location.
  // Keep marks across soft watcher reloads (Python keeps _new until the item
  // is gone / the directory is left). Only clear when navigating away.
  if (model_ != nullptr) {
    model_->clear_child_counts();
    QSet<QString> next_paths;
    next_paths.reserve(static_cast<int>(items.size()));
    for (const auto& fi : items) {
      next_paths.insert(QString::fromStdString(fi.path().string()));
    }
    if (known_paths_location_ != location_) {
      // Different folder (or first load): no "new" stickers for the initial set.
      model_->clear_new_marks();
    } else if (!known_paths_.empty()) {
      for (const QString& p : next_paths) {
        if (!known_paths_.contains(p)) {
          model_->mark_new(p);
        }
      }
      // Drop thumbs for paths that vanished; drop "new" marks for those too.
      for (const QString& p : known_paths_) {
        if (!next_paths.contains(p)) {
          model_->clear_thumbnail(p);
        }
      }
      model_->prune_new_marks(next_paths);
    }
    known_paths_ = next_paths;
    known_paths_location_ = location_;
  }
  collection_.sorter().set_ascending(sort_ascending_);
  const bool soft = soft_directory_reload_;
  soft_directory_reload_ = false;
  // Soft watcher refresh: merge into existing collection (preserve order of
  // survivors, append newcomers) instead of replacing the whole vector.
  const bool content_filter =
      filter_edit_ != nullptr && !filter_edit_->text().isEmpty()
      && filter_expression_needs_content_io(filter_edit_->text());
  if (soft && !collection_.empty()) {
    // Avoid rebuild_visible when content matchers would hit the GUI thread.
    collection_.merge_items(std::move(items), !content_filter);
  } else {
    collection_.set_items_unsorted(std::move(items));
  }
  if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()) {
    if (content_filter) {
      // Keep previous visible list until FilterWorker finishes (no empty flash).
      request_async_filter(/*keep_previous_visible=*/true);
    } else {
      collection_.set_name_filter(filter_edit_->text().toStdString());
    }
  }
  // Soft (watcher) reloads: keep the previous paint until sort finishes to avoid
  // unsorted→sorted double flicker. Hard navigation still paints ASAP.
  if (!soft) {
    refresh_list();
  }
  set_status(QStringLiteral("%1 items").arg(
      soft ? known_paths_.size() : collection_.visible_items().size()));
  // After soft path, visible may still be old until sort/filter apply; status updated again later.
  // Content filters own the visible list via FilterWorker; replace_items_sorted would
  // rebuild_visible with matchers (GUI I/O) or wipe the async filter result.
  if (!content_filter) {
    request_async_sort();
  }
  request_thumbnails_for_visible();
}

void MainWindow::update_detail_row_heights()
{
  if (tree_view_ == nullptr || model_ == nullptr) {
    return;
  }
  const bool variable =
      collection_.group_mode() != collection::GroupMode::None || model_->show_timegaps();
  tree_view_->setUniformRowHeights(!variable);
  tree_view_->doItemsLayout();
}

void MainWindow::request_async_sort()
{
  // Content filters own visible_ via FilterWorker. Sorting must not call
  // replace_items_sorted → rebuild_visible (GUI content I/O / wipe filter).
  if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()
      && filter_expression_needs_content_io(filter_edit_->text())) {
    collection_.sort_items_only();
    request_async_filter(/*keep_previous_visible=*/true);
    return;
  }
  if (sort_worker_ == nullptr) {
    collection_.apply_sort();
    refresh_list();
    return;
  }
  const quint64 gen = ++sort_generation_;
  auto items = collection_.items(); // copy
  const auto key = collection_.sorter().key();
  const bool asc = collection_.sorter().ascending();
  const bool dirs_first = collection_.sorter().directories_first();
  QMetaObject::invokeMethod(sort_worker_, "sort_items", Qt::QueuedConnection,
                            Q_ARG(std::vector<dirtoo::fs::FileInfo>, items),
                            Q_ARG(dirtoo::collection::SortKey, key), Q_ARG(bool, asc),
                            Q_ARG(bool, dirs_first), Q_ARG(quint64, gen));
}

void MainWindow::on_sort_finished(quint64 generation, std::vector<fs::FileInfo> items)
{
  if (generation != sort_generation_ || search_active_) {
    return;
  }
  // match_/filter and show_hidden stay; only item order changes.
  collection_.replace_items_sorted(std::move(items));
  refresh_list();
  set_status(QStringLiteral("%1 items").arg(collection_.visible_items().size()));
  request_thumbnails_for_visible();
}

void MainWindow::on_directory_load_failed(quint64 generation, QString error)
{
  qWarning().noquote() << QStringLiteral("directory_load_failed gen=%1: %2")
                              .arg(generation)
                              .arg(error);

  if (generation != dir_load_generation_) {
    return;
  }
  set_status(error);
  if (message_area_ != nullptr) {
    message_area_->show_error(error);
  }
}

void MainWindow::request_thumbnails_for_visible()
{
  // Debounce rapid refresh/scroll storms (generation-safe via singleShot capturing this).
  QTimer::singleShot(80, this, [this] {
    const auto& visible = collection_.visible_items();
    if (visible.empty()) {
      return;
    }

    // Prefer viewport rows when the active view can report them; fall back to a
    // capped prefix so huge directories do not queue thousands of D-Bus jobs.
    std::vector<int> rows;
    if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
      const QRect vp = graphics_view_->viewport()->rect().adjusted(-64, -64, 64, 64);
      const QRectF scene_rect = graphics_view_->mapToScene(vp).boundingRect();
      for (QGraphicsItem* gi : graphics_view_->scene()->items(scene_rect)) {
        if (auto* item = qgraphicsitem_cast<GraphicsFileItem*>(gi)) {
          rows.push_back(item->row());
        }
      }
      std::sort(rows.begin(), rows.end());
      rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    } else if (QAbstractItemView* view = current_view()) {
      const QRect vp = view->viewport()->rect();
      // Walk the viewport by approximate row step so Detail/List only queue
      // thumbnails for on-screen rows (plus a small pad), not a huge range.
      const int step = std::max(8, view->sizeHintForRow(0) > 0 ? view->sizeHintForRow(0) : 24);
      int first = model_->rowCount();
      int last = -1;
      for (int y = 0; y < vp.height(); y += step) {
        const QModelIndex idx = view->indexAt(QPoint(4, y));
        if (!idx.isValid()) {
          continue;
        }
        first = std::min(first, idx.row());
        last = std::max(last, idx.row());
      }
      if (last < 0) {
        const QModelIndex top = view->indexAt(vp.topLeft() + QPoint(4, 4));
        const QModelIndex bot = view->indexAt(vp.bottomLeft() + QPoint(4, -4));
        first = top.isValid() ? top.row() : 0;
        last = bot.isValid() ? bot.row() : first;
      }
      if (last < first) {
        std::swap(first, last);
      }
      first = std::max(0, first - 4);
      last = std::min(model_->rowCount() - 1, last + 8);
      for (int r = first; r <= last; ++r) {
        rows.push_back(r);
      }
    }

    constexpr int kMaxBatch = 64;
    if (rows.empty()) {
      const int n = std::min(static_cast<int>(visible.size()), kMaxBatch);
      rows.reserve(static_cast<std::size_t>(n));
      for (int r = 0; r < n; ++r) {
        rows.push_back(r);
      }
    } else if (static_cast<int>(rows.size()) > kMaxBatch) {
      rows.resize(static_cast<std::size_t>(kMaxBatch));
    }

    std::vector<fs::Location> locs;
    QStringList mimes;
    locs.reserve(rows.size());
    mimes.reserve(static_cast<int>(rows.size()));
    for (int r : rows) {
      if (r < 0 || static_cast<std::size_t>(r) >= visible.size()) {
        continue;
      }
      const auto& fi = visible[static_cast<std::size_t>(r)];
      if (fi.is_directory()) {
        // Use an existing XDG/cache montage if present; do not auto-generate here.
        const QString cache = thumbnail::Thumbnailer::cache_path_for(fi.location(),
                                                                     QStringLiteral("large"));
        if (QFile::exists(cache) && model_ != nullptr) {
          model_->set_thumbnail(QString::fromStdString(fi.path().string()), QIcon(cache));
        }
        continue;
      }
      // Archive members are synthetic; still thumbnailable after extract-to-cache.
      if (fi.is_synthetic() && !fi.location().is_archive()) {
        continue;
      }
      const QString model_key = QString::fromStdString(fi.path().string());
      if (model_ != nullptr) {
        if (model_->thumbnail_status(model_key) == ThumbnailStatus::Ready
            || model_->thumbnail_status(model_key) == ThumbnailStatus::Pending) {
          continue;
        }
        model_->set_thumbnail_pending(model_key);
      }
      static QMimeDatabase mime_db;
      const QString name_for_mime = QString::fromStdString(fi.basename());
      const QMimeType mt = mime_db.mimeTypeForFile(name_for_mime, QMimeDatabase::MatchExtension);
      const QString mime =
          mt.isValid() ? mt.name() : QStringLiteral("application/octet-stream");

      if (fi.location().is_archive()) {
        std::filesystem::path real;
        const fs::Location archive_root =
            fs::Location::from_archive(fi.location().as_path(), {});
        if (const auto root = archive_manager_.extracted_root(archive_root)) {
          real = *root / fi.location().entry_path();
        }
        if (real.empty() || !std::filesystem::exists(real)) {
          // Extract single member off the GUI thread, then ask Thumbnailer1.
          const auto archive_file = fi.location().as_path();
          const auto member = fi.location().entry_path();
          const QString key = model_key;
          const QString mime_copy = mime;
          const auto cache_root = archive_member_cache_root("dirtoo-archive-thumbs");
          (void)QtConcurrent::run([this, archive_file, member, key, mime_copy, cache_root]() {
            const auto dest_dir = archive_member_dest_dir(cache_root, archive_file);
            auto extracted =
                ensure_archive_member_extracted(archive_file, member, dest_dir);
            const bool ok = extracted.has_value();
            const std::filesystem::path out_path = ok ? *extracted : std::filesystem::path{};
            QMetaObject::invokeMethod(this, [this, ok, out_path, key, mime_copy]() {
              if (!ok) {
                if (model_ != nullptr) {
                  model_->set_thumbnail_failed(key);
                }
                return;
              }
              const QString real_path = QString::fromStdString(out_path.string());
              thumb_alias_.insert(real_path, key);
              thumbnailer_.request(fs::Location::from_path(out_path), mime_copy,
                                   QStringLiteral("large"));
            }, Qt::QueuedConnection);
          });
          continue;
        }
        const QString real_path = QString::fromStdString(real.string());
        thumb_alias_.insert(real_path, model_key);
        locs.push_back(fs::Location::from_path(real));
        mimes.push_back(mime);
        continue;
      }

      locs.push_back(fi.location());
      mimes.push_back(mime);
    }
    if (!locs.empty()) {
      qDebug().noquote() << QStringLiteral("thumbnails: requesting %1 (viewport/batch)")
                                .arg(locs.size());
      thumbnailer_.request_many(locs, mimes, QStringLiteral("large"));
    }
  });
}

void MainWindow::on_thumbnail_ready(const fs::Location& location, const QString& path)
{
  QPixmap pix(path);
  if (pix.isNull()) {
    return;
  }
  QString key = QString::fromStdString(location.as_path().string());
  if (const auto it = thumb_alias_.constFind(key); it != thumb_alias_.cend()) {
    key = it.value();
  }
  if (model_ != nullptr) {
    model_->set_thumbnail(key, QIcon(pix));
  }
}

void MainWindow::on_thumbnail_failed(const fs::Location& location, const QString& message)
{
  (void)message;
  QString key = QString::fromStdString(location.as_path().string());
  if (const auto it = thumb_alias_.constFind(key); it != thumb_alias_.cend()) {
    key = it.value();
  }
  if (model_ == nullptr) {
    return;
  }
  // Only keep a visible error/caution badge for types that normally produce
  // thumbnails. Text, binaries, etc. often fail the D-Bus thumbnailer — fall
  // back to the system icon without a warning sticker (dirtoo-py Unavailable).
  const QString name = QString::fromStdString(location.basename());
  static QMimeDatabase mime_db;
  const QMimeType mt = mime_db.mimeTypeForFile(name, QMimeDatabase::MatchExtension);
  const QString mime = mt.isValid() ? mt.name() : QString{};
  const bool expect_thumb =
      mime.startsWith(QLatin1String("image/")) || mime.startsWith(QLatin1String("video/"))
      || mime == QLatin1String("application/pdf")
      || mime.contains(QLatin1String("opendocument"))
      || mime.contains(QLatin1String("officedocument"));
  if (expect_thumb) {
    model_->set_thumbnail_failed(key);
  } else {
    model_->clear_thumbnail(key);
  }
}

void MainWindow::on_show_filter_help()
{
  auto* dlg = new QDialog(this);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setWindowTitle(QStringLiteral("Filter expression help"));
  dlg->resize(560, 720);
  auto* v = new QVBoxLayout(dlg);
  auto* browser = new QTextBrowser(dlg);
  browser->setOpenExternalLinks(false);
  browser->setHtml(QString::fromStdString(dirtoo::filter::filter_help_html()));
  v->addWidget(browser, 1);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
  connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
  // Close button maps to rejected for Close-only box
  connect(buttons, &QDialogButtonBox::clicked, dlg, [dlg](QAbstractButton*) { dlg->close(); });
  v->addWidget(buttons);
  dlg->show();
}

void MainWindow::update_filter_chrome(bool filtered)
{
  // Tint only the file views when a filter is active (dirtoo-py set_filtered).
  // Keep the filter bar on the window/chrome palette so it does not pick up the
  // view Base color (which made the bar look “merged” into the list).
  const QColor tint(220, 220, 255);
  const QColor app_base = qApp->palette().color(QPalette::Base);
  const QColor app_window = qApp->palette().color(QPalette::Window);

  auto apply_view_bg = [&](QWidget* w) {
    if (w == nullptr) {
      return;
    }
    QPalette pal = w->palette();
    pal.setColor(QPalette::Base, filtered ? tint : app_base);
    w->setPalette(pal);
  };
  apply_view_bg(tree_view_);
  apply_view_bg(icon_view_);
  if (graphics_view_ != nullptr) {
    if (filtered) {
      graphics_view_->setBackgroundBrush(QBrush(tint));
    } else {
      graphics_view_->setBackgroundBrush(QBrush(app_base));
    }
  }

  if (filter_row_ != nullptr) {
    // Explicit chrome background; never inherit the tinted view Base.
    filter_row_->setAutoFillBackground(true);
    QPalette bar_pal = filter_row_->palette();
    bar_pal.setColor(QPalette::Window, app_window);
    bar_pal.setColor(QPalette::Base, app_base);
    filter_row_->setPalette(bar_pal);
    filter_row_->setStyleSheet(QString());
  }
  if (filter_edit_ != nullptr) {
    QPalette edit_pal = filter_edit_->palette();
    edit_pal.setColor(QPalette::Base, app_base);
    edit_pal.setColor(QPalette::Window, app_base);
    filter_edit_->setPalette(edit_pal);
  }
}

void MainWindow::on_filter_changed(const QString& text)
{
  update_filter_chrome(!text.isEmpty());
  if (filter_expression_needs_content_io(text)) {
    // Content predicates must not run on the GUI thread.
    request_async_filter();
    return;
  }
  collection_.set_name_filter(text.toStdString());
  refresh_list();
  request_thumbnails_for_visible();
  if (!text.isEmpty() && !collection_.filter_parse_ok()) {
    if (message_area_ != nullptr) {
      message_area_->show_info(QStringLiteral("Filter parse issue — using substring fallback"));
    }
  }
}

void MainWindow::request_async_filter(bool keep_previous_visible)
{
  if (filter_worker_ == nullptr) {
    collection_.set_name_filter(
        filter_edit_ != nullptr ? filter_edit_->text().toStdString() : std::string{});
    refresh_list();
    return;
  }
  const quint64 gen = ++filter_generation_;
  set_status(QStringLiteral("Filtering…"));
  auto items = collection_.items();
  const QString expr = filter_edit_ != nullptr ? filter_edit_->text() : QString();
  const bool show_hidden = collection_.show_hidden();
  const auto group_mode = collection_.group_mode();
  // Avoid GUI-thread content I/O. For interactive filter changes, clear visible
  // until the worker finishes; for soft watcher reloads keep the previous list.
  if (!keep_previous_visible) {
    collection_.replace_visible({}, true);
    refresh_list();
  }
  QMetaObject::invokeMethod(
      filter_worker_, "filter_items", Qt::QueuedConnection,
      Q_ARG(std::vector<dirtoo::fs::FileInfo>, items), Q_ARG(QString, expr),
      Q_ARG(bool, show_hidden), Q_ARG(dirtoo::collection::GroupMode, group_mode),
      Q_ARG(quint64, gen));
}

void MainWindow::on_filter_finished(quint64 generation, std::vector<dirtoo::fs::FileInfo> visible,
                                    bool parse_ok)
{
  if (generation != filter_generation_ || search_active_) {
    return;
  }
  // Apply current sort to the filtered list (in-memory only; no FS I/O).
  collection_.sorter().sort(visible);
  collection_.replace_visible(std::move(visible), parse_ok);
  refresh_list();
  request_thumbnails_for_visible();
  if (!parse_ok && message_area_ != nullptr) {
    message_area_->show_info(QStringLiteral("Filter parse issue — using substring fallback"));
  }
  set_status(QStringLiteral("%1 items").arg(collection_.visible_items().size()));
}

void MainWindow::apply_sort_key(collection::SortKey key, bool toggle_if_same)
{
  if (toggle_if_same && sort_key_ == key) {
    sort_ascending_ = !sort_ascending_;
  } else {
    sort_key_ = key;
    sort_ascending_ = true;
  }
  collection_.sorter().set_key(key);
  collection_.sorter().set_ascending(sort_ascending_);
  update_sort_toolbar_label();
  if (tree_view_ != nullptr && tree_view_->header() != nullptr) {
    tree_view_->header()->setSortIndicatorShown(true);
    // Map SortKey → visible FileListColumn for indicator when possible.
    int section = static_cast<int>(FileListColumn::Name);
    switch (key) {
    case collection::SortKey::Size:
      section = static_cast<int>(FileListColumn::Size);
      break;
    case collection::SortKey::Width:
      section = static_cast<int>(FileListColumn::Width);
      break;
    case collection::SortKey::Height:
      section = static_cast<int>(FileListColumn::Height);
      break;
    case collection::SortKey::Resolution:
      section = static_cast<int>(FileListColumn::Dimensions);
      break;
    case collection::SortKey::AspectRatio:
      section = static_cast<int>(FileListColumn::AspectRatio);
      break;
    case collection::SortKey::Framerate:
      section = static_cast<int>(FileListColumn::Framerate);
      break;
    case collection::SortKey::Duration:
      section = static_cast<int>(FileListColumn::Duration);
      break;
    case collection::SortKey::Modified:
      section = static_cast<int>(FileListColumn::Modified);
      break;
    case collection::SortKey::Type:
    case collection::SortKey::Extension:
      section = static_cast<int>(FileListColumn::Type);
      break;
    default:
      section = static_cast<int>(FileListColumn::Name);
      break;
    }
    tree_view_->header()->setSortIndicator(
        section, sort_ascending_ ? Qt::AscendingOrder : Qt::DescendingOrder);
  }
  request_async_sort();
}

void MainWindow::update_sort_toolbar_label()
{
  if (sort_toolbar_btn_ == nullptr) {
    return;
  }
  QString title = QStringLiteral("Name");
  switch (sort_key_) {
  case collection::SortKey::Size:
    title = QStringLiteral("Size");
    break;
  case collection::SortKey::Modified:
    title = QStringLiteral("Modified");
    break;
  case collection::SortKey::Type:
  case collection::SortKey::Extension:
    title = QStringLiteral("Type");
    break;
  case collection::SortKey::Width:
    title = QStringLiteral("Width");
    break;
  case collection::SortKey::Height:
    title = QStringLiteral("Height");
    break;
  case collection::SortKey::Resolution:
    title = QStringLiteral("Dimensions");
    break;
  case collection::SortKey::AspectRatio:
    title = QStringLiteral("Aspect");
    break;
  case collection::SortKey::Duration:
    title = QStringLiteral("Duration");
    break;
  case collection::SortKey::Framerate:
    title = QStringLiteral("FPS");
    break;
  case collection::SortKey::Random:
    title = QStringLiteral("Random");
    break;
  default:
    title = QStringLiteral("Name");
    break;
  }
  sort_toolbar_btn_->setText(title);
}

void MainWindow::on_header_clicked(int section)
{
  using collection::SortKey;
  SortKey key = SortKey::Name;
  switch (static_cast<FileListColumn>(section)) {
  case FileListColumn::Name:
    key = SortKey::Name;
    break;
  case FileListColumn::Size:
    key = SortKey::Size;
    break;
  case FileListColumn::Width:
    key = SortKey::Width;
    break;
  case FileListColumn::Height:
    key = SortKey::Height;
    break;
  case FileListColumn::Dimensions:
    key = SortKey::Resolution; // width * height
    break;
  case FileListColumn::AspectRatio:
    key = SortKey::AspectRatio;
    break;
  case FileListColumn::Framerate:
    key = SortKey::Framerate;
    break;
  case FileListColumn::Duration:
    key = SortKey::Duration;
    break;
  case FileListColumn::Modified:
    key = SortKey::Modified;
    break;
  case FileListColumn::Type:
    key = SortKey::Type;
    break;
  case FileListColumn::Count:
    return;
  }
  apply_sort_key(key, /*toggle_if_same=*/true);
}

void MainWindow::on_item_activated(const QModelIndex& index)
{
  const fs::FileInfo* fi = model_->file_at(index.row());
  if (fi == nullptr) {
    return;
  }
  if (fi->is_directory()) {
    if (location_.is_archive()) {
      open_location(location_.join(fi->basename()));
    } else {
      open_location(fi->location());
    }
  } else if (location_.is_archive()) {
    // Extract single member then open with the default application.
    const auto member = location_.entry_path().empty()
                            ? std::filesystem::path{fi->basename()}
                            : location_.entry_path() / fi->basename();
    const auto cache = std::filesystem::temp_directory_path() / "dirtoo-open" /
                       std::to_string(std::hash<std::string>{}(location_.as_path().string()));
    auto extracted = archive::extract_member(location_.as_path(), member, cache);
    if (!extracted) {
      QMessageBox::warning(this, QStringLiteral("Archive"),
                           QString::fromStdString(extracted.error()));
      return;
    }
    if (fs::looks_like_archive(*extracted)) {
      open_location(fs::Location::from_archive(*extracted, {}));
    } else {
      open_default(*extracted);
    }
  } else if (fs::looks_like_archive(fi->path())) {
    open_location(fs::Location::from_archive(fi->path(), {}));
  } else {
    open_default(fi->path());
  }
}

std::vector<fs::FileInfo> MainWindow::selected_fileinfos() const
{
  if (model_ == nullptr) {
    return {};
  }
  if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
    std::vector<fs::FileInfo> out;
    for (int row : graphics_view_->selected_rows()) {
      if (const auto* fi = model_->file_at(row)) {
        out.push_back(*fi);
      }
    }
    return out;
  }
  auto* view = current_view();
  if (view == nullptr || view->selectionModel() == nullptr) {
    return {};
  }
  return model_->files_at(view->selectionModel()->selectedIndexes());
}

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

void MainWindow::on_cut()
{
  set_clipboard(ClipboardMode::Cut);
}

void MainWindow::start_transfer(const TransferRequest& request)
{
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

void MainWindow::on_paste()
{
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

void MainWindow::on_mkdir()
{
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

void MainWindow::on_toggle_show_abspath(bool checked)
{
  show_abspath_ = checked;
  if (model_ != nullptr) {
    model_->set_show_abspath(checked);
  }
  if (graphics_view_ != nullptr) {
    graphics_view_->viewport()->update();
  }
}

void MainWindow::on_rename_selected()
{
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

void MainWindow::refresh_list()
{
  model_->refresh();
  update_status_selection();
}


void MainWindow::on_properties()
{
  const auto selected = selected_fileinfos();
  if (selected.empty()) {
    set_status(QStringLiteral("Nothing selected"));
    return;
  }
  show_properties_dialog(this, selected);
}

void MainWindow::on_selection_changed()
{
  update_status_selection();
}

void MainWindow::update_status_selection()
{
  // Left: filename (or multi-selection summary). Right: directory size stats
  // (sum of listed item sizes — same idea as dirtoo-py controller._update_info).
  const auto& all = collection_.items();
  const auto& visible = collection_.visible_items();
  const auto selected = selected_fileinfos();

  std::uint64_t visible_bytes = 0;
  for (const auto& fi : visible) {
    visible_bytes += fi.size();
  }
  std::uint64_t total_bytes = 0;
  for (const auto& fi : all) {
    total_bytes += fi.size();
  }

  QString info = QStringLiteral("%1 visible (%2)")
                     .arg(visible.size())
                     .arg(format_byte_size(visible_bytes));
  if (all.size() != visible.size()) {
    info += QStringLiteral(", %1 total (%2)")
                .arg(all.size())
                .arg(format_byte_size(total_bytes));
  }

  if (!selected.empty()) {
    std::uint64_t selected_bytes = 0;
    for (const auto& fi : selected) {
      selected_bytes += fi.size();
    }
    info += QStringLiteral(", %1 selected (%2)")
                .arg(selected.size())
                .arg(format_byte_size(selected_bytes));
  }

  if (status_info_label_ != nullptr) {
    status_info_label_->setText(QStringLiteral("  ") + info);
  }

  if (selected.empty()) {
    // Clear left so transient set_status messages (Loading…, etc.) can show;
    // steady state with no selection leaves the right panel as the summary.
    if (status_label_ != nullptr) {
      status_label_->setText(QString());
    }
  } else if (selected.size() == 1) {
    set_status(QString::fromStdString(selected.front().path().string()));
  } else {
    set_status(QStringLiteral("%1 selected").arg(selected.size()));
  }
}

void MainWindow::on_urls_dropped(const QList<QUrl>& urls, Qt::DropAction action)
{
  on_urls_dropped_to(urls, action, {});
}

void MainWindow::on_select_all()
{
  if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
    // Must mark all model rows, not only live viewport tiles.
    graphics_view_->select_all();
    return;
  }
  if (QAbstractItemView* view = current_view()) {
    view->selectAll();
  }
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

void MainWindow::on_save_file_list()
{
  if (model_ == nullptr) {
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Save File List As"),
      QString::fromStdString(location_.as_path().string()) + QStringLiteral("/filelist.txt"),
      QStringLiteral("Text files (*.txt);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
    QMessageBox::warning(this, QStringLiteral("Save File List"),
                         QStringLiteral("Could not write %1").arg(path));
    return;
  }
  QTextStream out(&file);
  const int rows = model_->rowCount();
  for (int r = 0; r < rows; ++r) {
    if (const auto* fi = model_->file_at(r)) {
      out << QString::fromStdString(fi->path().string()) << QChar('\n');
    }
  }
  set_status(QStringLiteral("Saved %1 paths to %2").arg(rows).arg(path));
}

void MainWindow::restore_settings()
{
  const AppSettings s = load_settings();
  set_size_unit_style(size_unit_style_from_string(s.size_units));
  if (model_ != nullptr) {
    model_->set_icon_detail_level(s.icon_detail_level);
    model_->set_crop_thumbnails(s.crop_thumbnails);
  }
  if (crop_thumbnails_act_ != nullptr) {
    crop_thumbnails_act_->setChecked(s.crop_thumbnails);
  }
  zoom_icons_ = std::clamp(s.zoom_icons, 0, static_cast<int>(std::size(kZoomLevels)) - 1);
  zoom_list_ = std::clamp(s.zoom_list, 0, 6);
  zoom_detail_ = std::clamp(s.zoom_detail, 0, 6);
  if (!s.detail_columns.isEmpty()) {
    detail_columns_ = s.detail_columns;
  }
  apply_detail_column_visibility();
  apply_icon_zoom();
  collection_.set_show_hidden(s.show_hidden);
  if (show_hidden_act_ != nullptr) {
    show_hidden_act_->setChecked(s.show_hidden);
  }
  filter_pinned_ = s.filter_pinned;
  if (pin_filter_act_ != nullptr) {
    pin_filter_act_->setChecked(s.filter_pinned);
  }
  if (show_filter_act_ != nullptr) {
    show_filter_act_->setChecked(s.show_filter || s.filter_pinned);
  }
  // Toggle the whole filter row; keep the line edit itself always visible/enabled.
  if (filter_row_ != nullptr) {
    filter_row_->setVisible(s.show_filter || s.filter_pinned);
  }
  if (filter_edit_ != nullptr) {
    filter_edit_->setVisible(true);
    filter_edit_->setEnabled(true);
  }
  collection_.sorter().set_directories_first(s.directories_first);
  {
    collection::GroupMode gm = collection::GroupMode::None;
    const QString g = s.group_mode.toLower();
    if (g == QLatin1String("day")) {
      gm = collection::GroupMode::Day;
    } else if (g == QLatin1String("directory")) {
      gm = collection::GroupMode::Directory;
    } else if (g == QLatin1String("duration")) {
      gm = collection::GroupMode::Duration;
    }
    collection_.set_group_mode(gm);
    update_detail_row_heights();
    if (group_toolbar_btn_ != nullptr) {
      if (gm == collection::GroupMode::Day) {
        group_toolbar_btn_->setText(QStringLiteral("Day"));
      } else if (gm == collection::GroupMode::Directory) {
        group_toolbar_btn_->setText(QStringLiteral("Directory"));
      } else if (gm == collection::GroupMode::Duration) {
        group_toolbar_btn_->setText(QStringLiteral("Duration"));
      } else {
        group_toolbar_btn_->setText(QStringLiteral("None"));
      }
    }
  }
  if (s.view_mode == QLatin1String("icons")) {
    set_view_mode(ViewMode::Icons);
  } else if (s.view_mode == QLatin1String("small") || s.view_mode == QLatin1String("smallicons")) {
    set_view_mode(ViewMode::SmallIcons);
  } else {
    set_view_mode(ViewMode::Detail);
  }
  if (!s.window_geometry.isEmpty()) {
    restoreGeometry(s.window_geometry);
  }
  if (!s.window_state.isEmpty()) {
    restoreState(s.window_state);
  }
  // Restore persistent location history for the History menu.
  std::vector<fs::Location> unique;
  for (const QString& entry : s.location_history) {
    if (entry.isEmpty()) {
      continue;
    }
    try {
      if (entry.startsWith(QLatin1String("archive://"))
          || entry.startsWith(QLatin1String("file://"))) {
        unique.push_back(fs::Location::from_url(entry.toStdString()));
      } else {
        unique.push_back(fs::Location::from_path(entry.toStdString()));
      }
    } catch (...) {
    }
  }
  nav_history_.set_unique_locations(std::move(unique));
}

void MainWindow::persist_settings() const
{
  AppSettings s = load_settings(); // keep size_units and other offline prefs
  if (view_mode_ == ViewMode::Icons) {
    s.view_mode = QStringLiteral("icons");
  } else if (view_mode_ == ViewMode::SmallIcons) {
    s.view_mode = QStringLiteral("small");
  } else {
    s.view_mode = QStringLiteral("detail");
  }
  s.zoom_icons = zoom_icons_;
  s.zoom_list = zoom_list_;
  s.zoom_detail = zoom_detail_;
  s.zoom_index = zoom_icons_;
  s.detail_columns = detail_columns_;
  if (model_ != nullptr) {
    s.icon_detail_level = model_->icon_detail_level();
    s.crop_thumbnails = model_->crop_thumbnails();
  }
  s.show_hidden = collection_.show_hidden();
  s.show_filter = show_filter_act_ != nullptr && show_filter_act_->isChecked();
  s.show_sidebar = show_sidebar_act_ != nullptr && show_sidebar_act_->isChecked();
  if (main_splitter_ != nullptr) {
    const QList<int> sizes = main_splitter_->sizes();
    if (!sizes.isEmpty()) {
      s.sidebar_width = sizes[0];
    }
  }
  s.filter_pinned = filter_pinned_;
  s.directories_first = collection_.sorter().directories_first();
  s.size_units = size_unit_style_to_string(size_unit_style());
  switch (collection_.group_mode()) {
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
  s.window_geometry = saveGeometry();
  s.window_state = saveState();
  s.last_location = QString::fromStdString(location_.as_path().string());
  s.location_history.clear();
  for (const auto& loc : nav_history_.unique_locations()) {
    s.location_history.append(QString::fromStdString(loc.as_url()));
  }
  save_settings(s);
}

void MainWindow::showEvent(QShowEvent* event)
{
  QMainWindow::showEvent(event);
  // Tool buttons exist after the toolbar is realized.
  if (parent_act_ != nullptr) {
    for (auto* tb : findChildren<QToolButton*>()) {
      if (tb->defaultAction() == parent_act_) {
        tb->installEventFilter(this);
      }
    }
  }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
  persist_settings();
  QMainWindow::closeEvent(event);
}


void MainWindow::on_refresh()
{
  on_directory_changed();
  set_status(QStringLiteral("Refreshed"));
}

void MainWindow::on_focus_location()
{
  show_location_line_edit();
  location_edit_->setFocus(Qt::ShortcutFocusReason);
  location_edit_->selectAll();
}


MainWindow* MainWindow::open_new_window(const fs::Location& location)
{
  auto* win = new MainWindow;
  win->setAttribute(Qt::WA_DeleteOnClose);
  win->open_location(location);
  win->show();
  win->raise();
  win->activateWindow();
  return win;
}

void MainWindow::on_new_window()
{
  open_new_window(location_);
}

void MainWindow::on_breadcrumb_location_new_window(const fs::Location& location)
{
  open_new_window(location);
}

void MainWindow::on_breadcrumb_location(const fs::Location& location)
{
  open_location(location);
}

void MainWindow::on_location_edit_requested()
{
  show_location_line_edit();
  location_edit_->setFocus(Qt::MouseFocusReason);
  location_edit_->selectAll();
}

void MainWindow::show_location_buttons()
{
  if (location_buttons_ != nullptr) {
    location_buttons_->show();
  }
  if (location_edit_ != nullptr) {
    location_edit_->hide();
  }
}

void MainWindow::show_location_line_edit()
{
  if (location_buttons_ != nullptr) {
    location_buttons_->hide();
  }
  if (location_edit_ != nullptr) {
    location_edit_->show();
  }
}

void MainWindow::on_open_with()
{
  const auto selected = selected_fileinfos();
  if (selected.empty()) {
    set_status(QStringLiteral("Nothing selected"));
    return;
  }
  std::vector<std::filesystem::path> paths;
  for (const auto& fi : selected) {
    paths.push_back(fi.path());
  }
  // Prefer a listed default app when available; otherwise prompt for a command.
  QMimeDatabase db;
  const QMimeType mt = db.mimeTypeForFile(QString::fromStdString(paths.front().string()),
                                          QMimeDatabase::MatchExtension);
  const auto apps = apps_for_mime(mt.isValid() ? mt.name() : QStringLiteral("application/octet-stream"));
  if (!apps.empty()) {
    if (launch_desktop_app(apps.front(), paths)) {
      return;
    }
  }
  open_with_command_dialog(this, paths);
}

void MainWindow::on_open_terminal()
{
  std::filesystem::path dir = location_.as_path();
  const auto selected = selected_fileinfos();
  if (selected.size() == 1 && selected.front().is_directory()) {
    dir = selected.front().path();
  }
  if (!open_in_terminal(dir)) {
    set_status(QStringLiteral("Could not launch a terminal emulator"));
  }
}


void MainWindow::on_toggle_hidden(bool checked)
{
  const QString expr = filter_edit_ != nullptr ? filter_edit_->text() : QString();
  if (filter_expression_needs_content_io(expr)) {
    collection_.set_show_hidden(checked, false);
  if (directory_tree_model_ != nullptr) {
    directory_tree_model_->set_show_hidden(checked);
  }
    request_async_filter();
    return;
  }
  collection_.set_show_hidden(checked);
  if (directory_tree_model_ != nullptr) {
    directory_tree_model_->set_show_hidden(checked);
  }
  refresh_list();
  request_thumbnails_for_visible();
}


void MainWindow::on_toggle_filter_visible()
{
  if (show_filter_act_ == nullptr) {
    return;
  }
  show_filter_act_->toggle();
}


void MainWindow::on_location_text_edited(const QString& text)
{
  path_completion_pending_ = text;
  if (path_completion_timer_ != nullptr) {
    path_completion_timer_->start();
  }
}

void MainWindow::on_path_completion_timeout()
{
  if (path_completion_worker_ == nullptr || path_completion_thread_ == nullptr) {
    return;
  }
  const QString text = path_completion_pending_;
  if (text.isEmpty()) {
    if (path_completion_model_ != nullptr) {
      path_completion_model_->setStringList({});
    }
    return;
  }
  const quint64 id = ++path_completion_request_id_;
  QMetaObject::invokeMethod(
      path_completion_worker_,
      [worker = path_completion_worker_, id, text] { worker->complete(id, text); },
      Qt::QueuedConnection);
}

void MainWindow::on_path_completions_ready(quint64 request_id, const QString& longest,
                                           const QStringList& candidates)
{
  (void)longest;
  if (request_id != path_completion_request_id_) {
    return; // stale
  }
  if (path_completion_model_ == nullptr) {
    return;
  }
  path_completion_model_->setStringList(candidates);
  if (path_completer_ != nullptr && location_edit_ != nullptr && location_edit_->hasFocus()
      && !candidates.isEmpty()) {
    path_completer_->complete();
  }
}


void MainWindow::on_show_search()
{
  stop_search();
  if (search_edit_ == nullptr) {
    return;
  }
  if (search_row_ != nullptr) {
    search_row_->setVisible(true);
  } else if (search_edit_ != nullptr) {
    search_edit_->setVisible(true);
  }
  search_edit_->setFocus(Qt::ShortcutFocusReason);
  search_edit_->selectAll();
}

void MainWindow::stop_search()
{
  search_controller_.stop();
  search_batch_.clear();
}


void MainWindow::on_search_submitted()
{
  if (search_edit_ == nullptr) {
    return;
  }
  const QString expr = search_edit_->text().trimmed();
  if (expr.isEmpty()) {
    return;
  }
  if (location_.is_archive()) {
    set_status(QStringLiteral("Recursive search is not available inside archives"));
    return;
  }

  stop_search();
  search_results_.clear();
  search_batch_.clear();
  search_status_matched_ = 0;
  search_active_ = true;
  collection_.clear();
  collection_.clear_filter();
  refresh_list();

  const QString root = QString::fromStdString(location_.as_path().string());
  const bool show_hidden = show_hidden_act_ != nullptr && show_hidden_act_->isChecked();
  set_status(QStringLiteral("Searching…"));
  if (message_area_ != nullptr) {
    message_area_->show_info(QStringLiteral("Recursive search: %1").arg(expr));
  }
  search_controller_.start(root, expr, show_hidden, /*max_depth=*/-1);
}

void MainWindow::on_search_match(const QString& path, bool is_directory, quint64 size)
{
  if (!search_active_) {
    return;
  }
  // Synthetic only: avoid FileInfo::from_path (stat) on the GUI thread for every hit.
  const std::filesystem::path p{path.toStdString()};
  auto info = fs::FileInfo::synthetic(fs::Location::from_path(p), p.filename().string(),
                                      is_directory, size);
  search_results_.push_back(info);
  search_batch_.push_back(std::move(info));
  // Incremental inserts (batched) beat periodic full layoutChanged on large sets.
  // Early results flush soon for responsiveness; larger batches later reduce model churn.
  if (search_batch_.size() >= 48 || search_results_.size() < 24) {
    flush_search_batch();
  } else if (search_batch_.size() == 1) {
    // Schedule a deferred flush so a trickle of hits still paints without waiting
    // for a full batch (or search end).
    QTimer::singleShot(50, this, [this] {
      if (search_active_) {
        flush_search_batch();
      }
    });
  }
}

void MainWindow::flush_search_batch()
{
  if (search_batch_.empty()) {
    return;
  }
  const int count = static_cast<int>(search_batch_.size());
  collection_.append_visible_items(std::move(search_batch_));
  search_batch_.clear();
  if (model_ != nullptr) {
    model_->notify_rows_appended(count);
  } else {
    refresh_list();
  }
  update_status_selection();
}

void MainWindow::on_search_progress(quint64 visited, quint64 matched)
{
  (void)visited;
  // Throttle status updates — progress can fire very frequently on huge trees.
  if (matched == search_status_matched_) {
    return;
  }
  if (matched < 32 || matched - search_status_matched_ >= 32) {
    search_status_matched_ = matched;
    set_status(QStringLiteral("Searching… %1 matches").arg(matched));
  }
}

void MainWindow::on_search_finished(quint64 matched, quint64 visited, const QString& error)
{
  flush_search_batch();
  request_thumbnails_for_visible();
  if (!error.isEmpty() && error != QStringLiteral("cancelled")) {
    set_status(error);
    if (message_area_ != nullptr) {
      message_area_->show_info(error);
    }
  } else if (error == QStringLiteral("cancelled")) {
    set_status(
        QStringLiteral("Search cancelled — %1 matches (%2 visited)").arg(matched).arg(visited));
  } else {
    set_status(
        QStringLiteral("Search done — %1 matches (%2 visited)").arg(matched).arg(visited));
  }
  // Thread lifecycle owned by SearchController.
  // Keep search_active_ true so directory watcher does not wipe results until user navigates.
}

void MainWindow::on_about()
{
  show_about_dialog(this);
}


void MainWindow::start_watcher_for_location()
{
  watcher_.stop();
  if (location_.is_archive()) {
    // Watch the archive *file* (TOC / replacement) and the extract tree when
    // ready (member content under the cache).
    std::vector<std::filesystem::path> paths;
    paths.push_back(location_.as_path());
    if (const auto resolved = archive_manager_.resolved_directory(location_)) {
      paths.push_back(*resolved);
    }
    watcher_.set_location(fs::Location::from_archive(location_.as_path(), location_.entry_path()));
    watcher_.set_watch_paths(std::move(paths));
    watcher_.start();
    return;
  }
  watcher_.set_location(location_);
  watcher_.set_extra_paths({});
  watcher_.start();
}

void MainWindow::on_archive_ready(const fs::Location& archive_location,
                                  const std::filesystem::path& extracted_root)
{
  (void)extracted_root;
  QApplication::restoreOverrideCursor();
  // Refresh if we are still viewing this archive (or a path inside it).
  if (!location_.is_archive() || location_.as_path() != archive_location.as_path()) {
    return;
  }
  set_status(QStringLiteral("Archive ready — %1")
                             .arg(QString::fromStdString(archive_location.as_path().filename().string())));
  start_watcher_for_location();
  on_directory_changed();
}

void MainWindow::on_archive_failed(const fs::Location& archive_location, const QString& message)
{
  QApplication::restoreOverrideCursor();
  if (!location_.is_archive() || location_.as_path() != archive_location.as_path()) {
    return;
  }
  QMessageBox::warning(this, QStringLiteral("Archive"), message);
  set_status(message);
  // Fall back to parent directory so the user is not stuck on a failed archive view.
  open_location(fs::Location::from_path(archive_location.as_path().parent_path()), false);
}


void MainWindow::on_clear_filter()
{
  if (search_row_ != nullptr ? search_row_->isVisible()
      : (search_edit_ != nullptr && search_edit_->isVisible())) {
    stop_search();
    search_active_ = false;
    search_results_.clear();
    if (search_row_ != nullptr) {
      search_row_->hide();
    } else if (search_edit_ != nullptr) {
      search_edit_->hide();
    }
    search_edit_->clear();
    on_directory_changed();
    return;
  }
  if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()) {
    filter_edit_->clear();
    return;
  }
  if ((filter_row_ != nullptr ? filter_row_->isVisible()
         : (filter_edit_ != nullptr && filter_edit_->isVisible()))
      && !filter_pinned_
      && show_filter_act_ != nullptr && show_filter_act_->isChecked()) {
    show_filter_act_->setChecked(false);
    return;
  }
  if (location_edit_ != nullptr && location_edit_->isVisible()) {
    show_location_buttons();
  }
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


bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
  // Right-click on an already-selected row must not clear multi-selection.
  if (event->type() == QEvent::MouseButtonPress) {
    auto* me = static_cast<QMouseEvent*>(event);
    if (me->button() == Qt::RightButton) {
      QAbstractItemView* av = nullptr;
      if (tree_view_ != nullptr && obj == tree_view_->viewport()) {
        av = tree_view_;
      } else if (icon_view_ != nullptr && obj == icon_view_->viewport()) {
        av = icon_view_;
      }
      if (av != nullptr && av->selectionModel() != nullptr) {
        const QModelIndex under = av->indexAt(me->pos());
        if (under.isValid() && av->selectionModel()->isSelected(under)) {
          // Swallow the press so the view does not re-select (clearing multi-select).
          // Context menu still arrives via customContextMenuRequested.
          return true;
        }
      }
    }
  }

  if (event->type() == QEvent::MouseButtonRelease) {
    auto* me = static_cast<QMouseEvent*>(event);
    if (me->button() == Qt::MiddleButton && parent_act_ != nullptr) {
      if (auto* tb = qobject_cast<QToolButton*>(obj)) {
        if (tb->defaultAction() == parent_act_) {
          on_parent_new_window();
          return true;
        }
      }
    }
    if (me->button() == Qt::MiddleButton) {
      QModelIndex index;
      if (obj == tree_view_->viewport()) {
        index = tree_view_->indexAt(me->pos());
      } else if (obj == icon_view_->viewport()) {
        index = icon_view_->indexAt(me->pos());
      }
      if (index.isValid()) {
        on_view_middle_click(index);
        return true;
      }
    }
  }

  // Home/End + type-ahead + graphics file-cursor when a file view has focus.
  const bool is_file_view =
      (tree_view_ != nullptr && (obj == tree_view_ || obj == tree_view_->viewport()))
      || (icon_view_ != nullptr && (obj == icon_view_ || obj == icon_view_->viewport()))
      || (graphics_view_ != nullptr
          && (obj == graphics_view_ || obj == graphics_view_->viewport()));
  if (is_file_view && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Home && ke->modifiers() == Qt::NoModifier) {
      jump_to_row(0);
      return true;
    }
    if (ke->key() == Qt::Key_End && ke->modifiers() == Qt::NoModifier) {
      if (model_ != nullptr && model_->rowCount() > 0) {
        jump_to_row(model_->rowCount() - 1);
      }
      return true;
    }
    // Graphics Icons mode: forward cursor keys from the viewport into the view
    // (viewport often holds focus; sendEvent to the view avoids re-entering this
    // filter on graphics_view_ itself).
    if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr
        && obj == graphics_view_->viewport()) {
      const int k = ke->key();
      const bool cursor_key =
          k == Qt::Key_Left || k == Qt::Key_Right || k == Qt::Key_Up || k == Qt::Key_Down
          || k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Escape
          || (k == Qt::Key_Space && (ke->modifiers() & Qt::ControlModifier));
      if (cursor_key) {
        QCoreApplication::sendEvent(graphics_view_, ke);
        return true;
      }
    }
    // Type-ahead: printable text without Ctrl/Alt/Meta opens the leap overlay.
    if (!(ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        && !ke->text().isEmpty() && ke->text().at(0).isPrint()
        && !ke->text().at(0).isSpace()) {
      if (leap_widget_ != nullptr) {
        leap_widget_->show_with_text(ke->text());
      }
      return true;
    }
  }

  if (obj == search_edit_ && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Escape) {
      stop_search();
      search_active_ = false;
      search_results_.clear();
      if (search_row_ != nullptr) {
        search_row_->hide();
      } else if (search_edit_ != nullptr) {
        search_edit_->hide();
      }
      search_edit_->clear();
      on_directory_changed();
      return true;
    }
  }

  if (obj == filter_edit_ && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Up) {
      if (filter_history_.isEmpty()) {
        return false; // let QLineEdit handle cursor / default
      }
      if (filter_history_index_ < 0) {
        filter_history_index_ = filter_history_.size() - 1;
      } else if (filter_history_index_ > 0) {
        --filter_history_index_;
      }
      filter_edit_->setText(filter_history_.at(filter_history_index_));
      return true;
    }
    if (ke->key() == Qt::Key_Down) {
      if (filter_history_.isEmpty() || filter_history_index_ < 0) {
        return false;
      }
      if (filter_history_index_ + 1 < filter_history_.size()) {
        ++filter_history_index_;
        filter_edit_->setText(filter_history_.at(filter_history_index_));
      } else {
        filter_history_index_ = -1;
        filter_edit_->clear();
      }
      return true;
    }
    if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
      const QString text = filter_edit_->text();
      if (!text.isEmpty() && (filter_history_.isEmpty() || filter_history_.last() != text)) {
        filter_history_.append(text);
        if (filter_history_.size() > 50) {
          filter_history_.removeFirst();
        }
      }
      filter_history_index_ = -1;
    }
  }

  return QMainWindow::eventFilter(obj, event);
}

void MainWindow::on_view_middle_click(const QModelIndex& index)
{
  const fs::FileInfo* fi = model_->file_at(index.row());
  if (fi == nullptr) {
    return;
  }
  if (fi->is_directory()) {
    if (location_.is_archive()) {
      open_new_window(location_.join(fi->basename()));
    } else {
      open_new_window(fi->location());
    }
  } else if (fs::looks_like_archive(fi->path()) && !location_.is_archive()) {
    open_new_window(fs::Location::from_archive(fi->path(), {}));
  }
}


void MainWindow::on_show_leap()
{
  if (leap_widget_ != nullptr) {
    leap_widget_->show_and_focus();
  }
}

void MainWindow::jump_to_row(int row)
{
  if (model_ == nullptr || row < 0 || row >= model_->rowCount()) {
    return;
  }
  const QModelIndex idx = model_->index(row, 0);
  if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
    graphics_view_->select_row(row, true);
    // select_row materializes the item when needed; centre it in the viewport.
    const auto items = graphics_view_->scene()->selectedItems();
    if (!items.isEmpty()) {
      graphics_view_->ensureVisible(items.front(), 32, 32);
    } else if (row == 0) {
      graphics_view_->verticalScrollBar()->setValue(0);
    } else if (row >= model_->rowCount() - 1) {
      graphics_view_->verticalScrollBar()->setValue(
          graphics_view_->verticalScrollBar()->maximum());
    }
    return;
  }
  auto* view = current_view();
  if (view == nullptr || view->selectionModel() == nullptr) {
    return;
  }
  view->setCurrentIndex(idx);
  view->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  view->scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

void MainWindow::on_leap(const QString& text, bool forward, bool from_key)
{
  (void)from_key;
  if (text.isEmpty() || model_ == nullptr) {
    return;
  }
  const int rows = model_->rowCount();
  if (rows <= 0) {
    return;
  }

  int start = 0;
  if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
    if (graphics_view_->cursor_row() >= 0) {
      start = graphics_view_->cursor_row();
    } else {
      const auto sel = graphics_view_->selected_rows();
      if (!sel.empty()) {
        start = sel.front();
      }
    }
  } else if (auto* view = current_view(); view != nullptr && view->selectionModel() != nullptr) {
    const auto sel = view->selectionModel()->selectedIndexes();
    if (!sel.isEmpty()) {
      start = sel.first().row();
    }
  }

  const QString needle = text.toLower();
  auto matches = [&](int row) {
    const fs::FileInfo* fi = model_->file_at(row);
    if (fi == nullptr) {
      return false;
    }
    return QString::fromStdString(fi->basename()).toLower().startsWith(needle);
  };

  int found = -1;
  if (forward) {
    for (int i = 1; i <= rows; ++i) {
      const int row = (start + i) % rows;
      if (matches(row)) {
        found = row;
        break;
      }
    }
  } else {
    for (int i = 1; i <= rows; ++i) {
      const int row = (start - i + rows * 2) % rows;
      if (matches(row)) {
        found = row;
        break;
      }
    }
  }

  if (found < 0) {
    return;
  }
  jump_to_row(found);
}

void MainWindow::on_parent_new_window()
{
  open_new_window(location_.parent());
}

void MainWindow::on_rebuild_history_menu()
{
  if (history_menu_ == nullptr) {
    return;
  }
  history_menu_->clear();
  history_menu_->addAction(theme_icon("go-previous", "arrow-left"), QStringLiteral("Back"), this,
                           &MainWindow::on_go_back);
  history_menu_->addAction(theme_icon("go-next", "arrow-right"), QStringLiteral("Forward"), this,
                           &MainWindow::on_go_forward);
  history_menu_->addSeparator();

  // Folder / location history — most recent first.
  std::vector<fs::Location> locs;
  int count = 0;
  for (auto it = nav_history_.unique_locations().rbegin();
       it != nav_history_.unique_locations().rend() && count < 35; ++it, ++count) {
    locs.push_back(*it);
  }
  add_location_menu_entries(
      history_menu_, locs, this,
      [this](const fs::Location& loc) { open_location(loc); },
      [this](const fs::Location& loc) { open_new_window(loc); });
}

void MainWindow::on_rebuild_recent_opens_menu()
{
  if (recent_opens_menu_ == nullptr) {
    return;
  }
  populate_recent_opens_menu(recent_opens_menu_, 30);
  recent_opens_menu_->addSeparator();
  recent_opens_menu_->addAction(theme_icon("view-history", "document-open-recent"),
                                QStringLiteral("Open History…"), this, [this] {
                                  show_open_history_dialog(this, [this](const QString& dir) {
                                    open_location(fs::Location::from_path(dir.toStdString()));
                                  });
                                });
}


void MainWindow::on_reload_thumbnails()
{
  if (model_ == nullptr) {
    return;
  }
  thumbnailer_.cancel_all();
  auto selected = selected_fileinfos();
  if (selected.empty()) {
    // Reload all visible
    model_->clear_thumbnails();
    request_thumbnails_for_visible();
    set_status(QStringLiteral("Reloading thumbnails for visible items…"));
    return;
  }
  for (const auto& fi : selected) {
    if (fi.is_directory() || fi.is_synthetic()) {
      continue;
    }
    model_->clear_thumbnail(QString::fromStdString(fi.path().string()));
  }
  // Re-request only selection by temporarily using visible-style request on those paths
  QMimeDatabase mime_db;
  std::vector<fs::Location> locs;
  QStringList mimes;
  for (const auto& fi : selected) {
    if (fi.is_directory() || fi.is_synthetic() || location_.is_archive()) {
      continue;
    }
    const QString path = QString::fromStdString(fi.path().string());
    model_->set_thumbnail_pending(path);
    locs.push_back(fi.location());
    mimes.push_back(mime_db.mimeTypeForFile(path).name());
  }
  if (!locs.empty()) {
    thumbnailer_.request_many(locs, mimes, QStringLiteral("large"));
  }
  set_status(QStringLiteral("Reloading %1 thumbnail(s)…").arg(locs.size()));
}


void MainWindow::on_make_directory_thumbnails()
{
  if (dir_thumb_worker_ == nullptr) {
    return;
  }
  QStringList dirs;
  auto selected = selected_fileinfos();
  if (selected.empty()) {
    // All visible directories
    for (const auto& fi : collection_.visible_items()) {
      if (fi.is_directory() && !fi.is_synthetic()) {
        dirs << QString::fromStdString(fi.path().string());
      }
    }
  } else {
    for (const auto& fi : selected) {
      if (fi.is_directory() && !fi.is_synthetic()) {
        dirs << QString::fromStdString(fi.path().string());
      }
    }
  }
  if (dirs.isEmpty()) {
    set_status(QStringLiteral("No directories to thumbnail"));
    return;
  }
  set_status(QStringLiteral("Building %1 directory thumbnail(s)…").arg(dirs.size()));
  for (const QString& d : dirs) {
    if (model_ != nullptr) {
      model_->set_thumbnail_pending(d);
    }
  }
  QMetaObject::invokeMethod(dir_thumb_worker_, "generate", Qt::QueuedConnection,
                            Q_ARG(QStringList, dirs));
}

void MainWindow::on_prepare_thumbnails()
{
  if (view_mode_ == ViewMode::Detail) {
    // Still useful: switch-less prepare for when user opens icons next
  }
  request_thumbnails_for_visible();
  set_status(QStringLiteral("Preparing thumbnails for visible items…"));
}

void MainWindow::on_preferences()
{
  AppSettings s = load_settings();
  // Reflect live UI into the struct before editing.
  if (view_mode_ == ViewMode::Icons) {
    s.view_mode = QStringLiteral("icons");
  } else if (view_mode_ == ViewMode::SmallIcons) {
    s.view_mode = QStringLiteral("small");
  } else {
    s.view_mode = QStringLiteral("detail");
  }
  s.zoom_icons = zoom_icons_;
  s.zoom_list = zoom_list_;
  s.zoom_detail = zoom_detail_;
  s.zoom_index = zoom_icons_;
  s.detail_columns = detail_columns_;
  if (model_ != nullptr) {
    s.icon_detail_level = model_->icon_detail_level();
    s.crop_thumbnails = model_->crop_thumbnails();
  }
  s.show_hidden = collection_.show_hidden();
  s.show_filter = show_filter_act_ != nullptr && show_filter_act_->isChecked();
  s.filter_pinned = filter_pinned_;
  s.directories_first = collection_.sorter().directories_first();
  switch (collection_.group_mode()) {
  case collection::GroupMode::Day:
    s.group_mode = QStringLiteral("day");
    break;
  case collection::GroupMode::Directory:
    s.group_mode = QStringLiteral("directory");
    break;
  case collection::GroupMode::Duration:
    s.group_mode = QStringLiteral("duration");
    break;
  case collection::GroupMode::None:
  default:
    s.group_mode = QStringLiteral("none");
    break;
  }
  if (!show_preferences_dialog(this, &s)) {
    return;
  }
  save_settings(s);
  apply_settings(s);
}

void MainWindow::apply_settings(const AppSettings& s)
{
  set_size_unit_style(size_unit_style_from_string(s.size_units));
  if (model_ != nullptr) {
    model_->set_icon_detail_level(s.icon_detail_level);
    model_->set_crop_thumbnails(s.crop_thumbnails);
    if (crop_thumbnails_act_ != nullptr) {
      crop_thumbnails_act_->setChecked(s.crop_thumbnails);
    }
  }
  zoom_icons_ = std::clamp(s.zoom_icons, 0, static_cast<int>(std::size(kZoomLevels)) - 1);
  zoom_list_ = std::clamp(s.zoom_list, 0, 6);
  zoom_detail_ = std::clamp(s.zoom_detail, 0, 6);
  if (!s.detail_columns.isEmpty()) {
    detail_columns_ = s.detail_columns;
  }
  apply_detail_column_visibility();
  apply_icon_zoom();
  if (s.view_mode == QLatin1String("icons")) {
    set_view_mode(ViewMode::Icons);
  } else if (s.view_mode == QLatin1String("small") || s.view_mode == QLatin1String("smallicons")) {
    set_view_mode(ViewMode::SmallIcons);
  } else {
    set_view_mode(ViewMode::Detail);
  }
  collection_.set_show_hidden(s.show_hidden);
  if (show_hidden_act_ != nullptr) {
    show_hidden_act_->setChecked(s.show_hidden);
  }
  collection_.sorter().set_directories_first(s.directories_first);
  {
    collection::GroupMode gm = collection::GroupMode::None;
    const QString g = s.group_mode.toLower();
    if (g == QLatin1String("day")) {
      gm = collection::GroupMode::Day;
    } else if (g == QLatin1String("directory")) {
      gm = collection::GroupMode::Directory;
    } else if (g == QLatin1String("duration")) {
      gm = collection::GroupMode::Duration;
    }
    collection_.set_group_mode(gm);
    update_detail_row_heights();
  }
  filter_pinned_ = s.filter_pinned;
  if (pin_filter_act_ != nullptr) {
    pin_filter_act_->setChecked(s.filter_pinned);
  }
  if (show_filter_act_ != nullptr) {
    show_filter_act_->setChecked(s.show_filter || s.filter_pinned);
  }
  if (filter_row_ != nullptr) {
    filter_row_->setVisible(s.show_filter || s.filter_pinned);
  }
  if (show_sidebar_act_ != nullptr) {
    show_sidebar_act_->setChecked(s.show_sidebar);
  }
  if (sidebar_widget_ != nullptr) {
    sidebar_widget_->setVisible(s.show_sidebar);
  }
  if (main_splitter_ != nullptr && s.sidebar_width > 40) {
    QList<int> sizes = main_splitter_->sizes();
    if (sizes.size() >= 2) {
      sizes[0] = s.sidebar_width;
      main_splitter_->setSizes(sizes);
    }
  }
  if (directory_tree_model_ != nullptr) {
    directory_tree_model_->set_show_hidden(s.show_hidden);
  }
  if (filter_edit_ != nullptr) {
    filter_edit_->setVisible(true);
    filter_edit_->setEnabled(true);
  }
  request_async_sort();
  refresh_list();
}


void MainWindow::on_toggle_bookmark()
{
  if (location_.empty()) {
    return;
  }
  const bool now = bookmarks_.toggle(location_);
  if (message_area_ != nullptr) {
    if (now) {
      message_area_->show_info(QStringLiteral("Bookmark added"));
    } else {
      message_area_->show_info(QStringLiteral("Bookmark removed"));
    }
  }
  set_status(now ? QStringLiteral("Bookmarked") : QStringLiteral("Bookmark removed"));
  rebuild_sidebar_places();
}

void MainWindow::on_rebuild_bookmarks_menu()
{
  if (bookmarks_menu_ == nullptr) {
    return;
  }
  bookmarks_menu_->clear();

  if (location_.empty()) {
    auto* act = bookmarks_menu_->addAction(QStringLiteral("No location to bookmark"));
    act->setEnabled(false);
  } else if (bookmarks_.contains(location_)) {
    bookmarks_menu_->addAction(theme_icon("bookmark-remove", "list-remove"),
                               QStringLiteral("Remove Bookmark for This Location"), this,
                               &MainWindow::on_toggle_bookmark);
  } else {
    bookmarks_menu_->addAction(theme_icon("bookmark-new", "list-add"),
                               QStringLiteral("Bookmark This Location"), this,
                               &MainWindow::on_toggle_bookmark);
  }
  bookmarks_menu_->addSeparator();

  const auto entries = bookmarks_.entries();
  if (entries.empty()) {
    auto* act = bookmarks_menu_->addAction(QStringLiteral("(no bookmarks yet)"));
    act->setEnabled(false);
    return;
  }

  add_location_menu_entries(
      bookmarks_menu_, entries, this,
      [this](const fs::Location& loc) { open_location(loc); },
      [this](const fs::Location& loc) { open_new_window(loc); });
}


void MainWindow::on_toggle_sidebar(bool checked)
{
  if (sidebar_widget_ != nullptr) {
    sidebar_widget_->setVisible(checked);
  }
  if (main_splitter_ != nullptr && checked && sidebar_widget_ != nullptr) {
    // Restore a usable width if collapsed
    QList<int> sizes = main_splitter_->sizes();
    if (sizes.size() >= 2 && sizes[0] < 80) {
      sizes[0] = 220;
      main_splitter_->setSizes(sizes);
    }
  }
}

void MainWindow::on_sidebar_activated(const QModelIndex& index)
{
  if (!index.isValid() || directory_tree_model_ == nullptr) {
    return;
  }
  const QString path = directory_tree_model_->path_for_index(index);
  if (path.isEmpty()) {
    return;
  }
  open_location(fs::Location::from_path(std::filesystem::path(path.toStdString())), true);
}

void MainWindow::sync_sidebar_to_location()
{
  if (sidebar_tree_ == nullptr || directory_tree_model_ == nullptr || sidebar_widget_ == nullptr
      || !sidebar_widget_->isVisible()) {
    return;
  }
  if (!location_.is_file()) {
    return;
  }
  const QString path = QString::fromStdString(location_.as_path().string());
  const QModelIndex ix = directory_tree_model_->ensure_path_visible(path);
  if (!ix.isValid()) {
    return;
  }
  for (QModelIndex parent = ix.parent(); parent.isValid(); parent = parent.parent()) {
    sidebar_tree_->expand(parent);
  }
  sidebar_tree_->expand(ix);
  sidebar_tree_->setCurrentIndex(ix);
  sidebar_tree_->scrollTo(ix, QAbstractItemView::PositionAtCenter);
}


void MainWindow::rebuild_sidebar_places()
{
  if (directory_tree_model_ == nullptr) {
    return;
  }
  QStringList roots;
  QStringList labels;
  const QString home = QDir::homePath();
  roots << home;
  labels << QStringLiteral("Home");
  roots << QStringLiteral("/");
  labels << QStringLiteral("Filesystem");
  for (auto loc : {QStandardPaths::DesktopLocation, QStandardPaths::DocumentsLocation,
                   QStandardPaths::DownloadLocation, QStandardPaths::MusicLocation,
                   QStandardPaths::PicturesLocation, QStandardPaths::MoviesLocation}) {
    const QString path = QStandardPaths::writableLocation(loc);
    if (!path.isEmpty() && QDir(path).exists() && path != home && !roots.contains(path)) {
      roots << path;
      labels << QStandardPaths::displayName(loc);
    }
  }
  for (const auto& loc : bookmarks_.entries()) {
    if (!loc.is_file()) {
      continue;
    }
    const QString path = QString::fromStdString(loc.as_path().string());
    if (path.isEmpty() || roots.contains(path)) {
      continue;
    }
    roots << path;
    labels << QFileInfo(path).fileName();
  }
  directory_tree_model_->reset_roots(roots, labels);
}


void MainWindow::apply_detail_column_visibility()
{
  if (tree_view_ == nullptr || model_ == nullptr) {
    return;
  }
  // Name (0) always visible.
  auto visible = [this](const char* key) {
    if (detail_columns_.isEmpty()) {
      // Defaults: everything except optional Width/Height.
      return QLatin1String(key) != QLatin1String("width")
          && QLatin1String(key) != QLatin1String("height");
    }
    return detail_columns_.contains(QLatin1String(key));
  };
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Size), !visible("size"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Width), !visible("width"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Height), !visible("height"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Dimensions), !visible("dimensions"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::AspectRatio), !visible("aspectratio"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Framerate), !visible("framerate"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Duration), !visible("duration"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Modified), !visible("modified"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Type), !visible("type"));
}


} // namespace dirtoo::app

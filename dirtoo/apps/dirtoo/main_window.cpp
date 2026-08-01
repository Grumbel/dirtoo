// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"
#include "file_views.hpp"
#include "file_item_delegate.hpp"
#include "dirtoo/filter/media_meta_cache.hpp"

#include "dirtoo/filter/parser.hpp"

#include "clipboard.hpp"
#include "about_dialog.hpp"
#include "app_settings.hpp"
#include "conflict_dialog.hpp"
#include "open_with.hpp"
#include "preferences_dialog.hpp"
#include "properties_dialog.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirops/ops.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QShowEvent>
#include <QCompleter>
#include <QStringListModel>
#include <QTimer>
#include <QEvent>
#include <QMouseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QItemSelectionModel>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPixmap>
#include <QProcess>
#include <QStackedWidget>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <filesystem>

namespace dirtoo::app {

namespace {

QIcon theme_icon(const char* name, const char* fallback = nullptr)
{
  QIcon icon = QIcon::fromTheme(QString::fromUtf8(name));
  if (icon.isNull() && fallback != nullptr) {
    icon = QIcon::fromTheme(QString::fromUtf8(fallback));
  }
  return icon;
}

} // namespace


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
  setWindowTitle(QStringLiteral("dirtoo"));
  resize(960, 640);

  // Background transfer worker
  transfer_worker_ = new TransferWorker;
  transfer_worker_->moveToThread(&transfer_thread_);
  connect(&transfer_thread_, &QThread::finished, transfer_worker_, &QObject::deleteLater);
  connect(transfer_worker_, &TransferWorker::item_started, this,
          &MainWindow::on_transfer_item_started);
  connect(transfer_worker_, &TransferWorker::byte_progress, this,
          &MainWindow::on_transfer_byte_progress);
  connect(transfer_worker_, &TransferWorker::conflict_required, this,
          &MainWindow::on_transfer_conflict);
  connect(transfer_worker_, &TransferWorker::finished, this, &MainWindow::on_transfer_finished);
  transfer_thread_.start();

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

  auto* toolbar = addToolBar(QStringLiteral("Main"));
  toolbar->setMovable(false);
  toolbar->setIconSize(QSize(24, 24));
  toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

  back_act_ = toolbar->addAction(theme_icon("go-previous", "arrow-left"), QStringLiteral("Back"), this,
                                 &MainWindow::on_go_back);
  forward_act_ = toolbar->addAction(theme_icon("go-next", "arrow-right"), QStringLiteral("Forward"), this,
                                    &MainWindow::on_go_forward);
  parent_act_ = toolbar->addAction(theme_icon("go-up", "arrow-up"), QStringLiteral("Parent"), this,
                                   &MainWindow::on_go_parent);
  // Middle-click Parent → open parent in a new window.
  if (auto* btn = qobject_cast<QToolButton*>(toolbar->widgetForAction(parent_act_))) {
    btn->installEventFilter(this);
  }
  toolbar->addAction(theme_icon("go-home", "user-home"), QStringLiteral("Home"), this, &MainWindow::on_go_home);
  toolbar->addSeparator();
  toolbar->addAction(theme_icon("folder-new"), QStringLiteral("New Folder"), this, &MainWindow::on_mkdir);
  toolbar->addSeparator();
  toolbar->addAction(theme_icon("edit-cut"), QStringLiteral("Cut"), this, &MainWindow::on_cut);
  toolbar->addAction(theme_icon("edit-copy"), QStringLiteral("Copy"), this, &MainWindow::on_copy);
  paste_act_ = toolbar->addAction(theme_icon("edit-paste"), QStringLiteral("Paste"), this, &MainWindow::on_paste);
  toolbar->addSeparator();

  detail_act_ = toolbar->addAction(theme_icon("view-list-details", "view-list"), QStringLiteral("Detail"));
  icons_act_ = toolbar->addAction(theme_icon("view-grid", "view-list-icons"), QStringLiteral("Icons"));
  small_icons_act_ = toolbar->addAction(theme_icon("view-list", "view-list-details"),
                                        QStringLiteral("Small Icons"));
  detail_act_->setCheckable(true);
  icons_act_->setCheckable(true);
  small_icons_act_->setCheckable(true);
  auto* view_group = new QActionGroup(this);
  view_group->addAction(detail_act_);
  view_group->addAction(icons_act_);
  view_group->addAction(small_icons_act_);
  connect(detail_act_, &QAction::triggered, this, &MainWindow::on_view_detail);
  connect(icons_act_, &QAction::triggered, this, &MainWindow::on_view_icons);
  connect(small_icons_act_, &QAction::triggered, this, &MainWindow::on_view_small_icons);
  detail_act_->setChecked(true);

  toolbar->addSeparator();
  toolbar->addAction(theme_icon("zoom-out"), QStringLiteral("Zoom −"), this, &MainWindow::on_zoom_out);
  toolbar->addAction(theme_icon("zoom-in"), QStringLiteral("Zoom +"), this, &MainWindow::on_zoom_in);
  {
    auto* act = toolbar->addAction(theme_icon("zoom-fit-best"), QStringLiteral("Crop Thumbnails"));
    act->setCheckable(true);
    act->setToolTip(QStringLiteral("Crop thumbnails to fill the icon (cover) instead of letterboxing"));
    connect(act, &QAction::toggled, this, [this](bool on) {
      if (model_ != nullptr) {
        model_->set_crop_thumbnails(on);
      }
      if (icon_view_ != nullptr) {
        icon_view_->viewport()->update();
      }
    });
    crop_thumbnails_act_ = act;
  }
  toolbar->addSeparator();
  toolbar->addAction(theme_icon("zoom-original", "list-remove"), QStringLiteral("Less detail"), this,
                     &MainWindow::on_less_icon_details);
  toolbar->addAction(theme_icon("zoom-fit-best", "list-add"), QStringLiteral("More detail"), this,
                     &MainWindow::on_more_icon_details);

  // Menu bar
  {
    auto* file_menu = menuBar()->addMenu(QStringLiteral("&File"));
    {
      auto* act = file_menu->addAction(QStringLiteral("New Window"), this, &MainWindow::on_new_window);
      act->setShortcut(QKeySequence::New); // Ctrl+N
    }
    file_menu->addAction(QStringLiteral("New Folder…"), this, &MainWindow::on_mkdir);
    file_menu->addSeparator();
    {
      auto* act = file_menu->addAction(QStringLiteral("Close"), this, &QWidget::close);
      act->setShortcut(QKeySequence::Close);
    }

    auto* edit_menu = menuBar()->addMenu(QStringLiteral("&Edit"));
    {
      auto* act = edit_menu->addAction(QStringLiteral("Cut"), this, &MainWindow::on_cut);
      act->setShortcut(QKeySequence::Cut);
    }
    {
      auto* act = edit_menu->addAction(QStringLiteral("Copy"), this, &MainWindow::on_copy);
      act->setShortcut(QKeySequence::Copy);
    }
    {
      auto* act = edit_menu->addAction(QStringLiteral("Paste"), this, &MainWindow::on_paste);
      act->setShortcut(QKeySequence::Paste);
    }
    edit_menu->addSeparator();
    {
      auto* act = edit_menu->addAction(QStringLiteral("Rename…"), this, &MainWindow::on_rename_selected);
      act->setShortcut(QKeySequence(Qt::Key_F2));
    }
    {
      auto* act = edit_menu->addAction(QStringLiteral("Delete…"), this, &MainWindow::on_delete_selected);
      act->setShortcut(QKeySequence::Delete);
    }
    {
      auto* act = edit_menu->addAction(QStringLiteral("Properties…"), this, &MainWindow::on_properties);
      act->setShortcut(QKeySequence(Qt::Key_F3));
    }
    edit_menu->addSeparator();
    {
      auto* act = edit_menu->addAction(QStringLiteral("Preferences…"), this, &MainWindow::on_preferences);
      act->setShortcut(QKeySequence::Preferences);
    }

    auto* view_menu = menuBar()->addMenu(QStringLiteral("&View"));
    view_menu->addAction(detail_act_);
    view_menu->addAction(icons_act_);
    view_menu->addAction(small_icons_act_);
    view_menu->addSeparator();
    show_hidden_act_ = view_menu->addAction(QStringLiteral("Show Hidden Files"));
    show_hidden_act_->setCheckable(true);
    show_hidden_act_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+H")));
    connect(show_hidden_act_, &QAction::toggled, this, &MainWindow::on_toggle_hidden);
    view_menu->addSeparator();
    show_filter_act_ = view_menu->addAction(QStringLiteral("Show Filter"));
    show_filter_act_->setCheckable(true);
    show_filter_act_->setChecked(true);
    show_filter_act_->setShortcut(QKeySequence(QStringLiteral("Ctrl+F")));
    connect(show_filter_act_, &QAction::toggled, this, [this](bool on) {
      if (filter_edit_ != nullptr) {
        filter_edit_->setVisible(on);
        if (on) {
          filter_edit_->setFocus(Qt::ShortcutFocusReason);
        }
      }
    });
    pin_filter_act_ = view_menu->addAction(QStringLiteral("Pin Filter"));
    pin_filter_act_->setCheckable(true);
    pin_filter_act_->setShortcut(QKeySequence(QStringLiteral("Ctrl+P")));
    connect(pin_filter_act_, &QAction::toggled, this, [this](bool on) {
      filter_pinned_ = on;
      if (on && show_filter_act_ != nullptr && !show_filter_act_->isChecked()) {
        show_filter_act_->setChecked(true);
      }
    });
    {
      auto* act = view_menu->addAction(QStringLiteral("Jump to…"), this, &MainWindow::on_show_leap);
      act->setShortcut(QKeySequence(QStringLiteral("/")));
    }
    {
      auto* act = view_menu->addAction(QStringLiteral("Refresh"), this, &MainWindow::on_refresh);
      act->setShortcut(QKeySequence(Qt::Key_F5));
    }
    {
      auto* act = view_menu->addAction(QStringLiteral("Reload Thumbnails"), this,
                                       &MainWindow::on_reload_thumbnails);
      act->setStatusTip(QStringLiteral("Clear and re-request thumbnails for the selection (or all visible)"));
    }
    {
      auto* act = view_menu->addAction(QStringLiteral("Prepare Thumbnails"), this,
                                       &MainWindow::on_prepare_thumbnails);
      act->setStatusTip(QStringLiteral("Request thumbnails for all visible items"));
    }
    {
      auto* act = view_menu->addAction(QStringLiteral("Recursive Search…"), this,
                                       &MainWindow::on_show_search);
      act->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));
    }
    {
      auto* act = view_menu->addAction(QStringLiteral("Zoom In"), this, &MainWindow::on_zoom_in);
      act->setShortcut(QKeySequence::ZoomIn);
    }
    {
      auto* act = view_menu->addAction(QStringLiteral("Zoom Out"), this, &MainWindow::on_zoom_out);
      act->setShortcut(QKeySequence::ZoomOut);
    }
    {
      auto* act = view_menu->addAction(QStringLiteral("Crop Thumbnails"));
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
      auto* act = view_menu->addAction(QStringLiteral("More Icon Details"), this,
                                       &MainWindow::on_more_icon_details);
      act->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+=")));
    }
    {
      auto* act = view_menu->addAction(QStringLiteral("Less Icon Details"), this,
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
        sort_ascending_ = true;
        collection_.sorter().set_ascending(true);
        collection_.sorter().set_key(key);
        // Map header-related keys
        switch (key) {
        case dirtoo::collection::SortKey::Name:
          sort_column_ = SortColumn::Name;
          break;
        case dirtoo::collection::SortKey::Size:
          sort_column_ = SortColumn::Size;
          break;
        case dirtoo::collection::SortKey::Modified:
          sort_column_ = SortColumn::Modified;
          break;
        case dirtoo::collection::SortKey::Type:
        case dirtoo::collection::SortKey::Extension:
          sort_column_ = SortColumn::Type;
          break;
        default:
          break;
        }
        request_async_sort();
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
      auto* act = sort_menu->addAction(QStringLiteral("Directories First"));
      act->setCheckable(true);
      act->setChecked(true);
      connect(act, &QAction::toggled, this, [this](bool on) {
        collection_.sorter().set_directories_first(on);
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
    go_menu->addAction(QStringLiteral("Back"), this, &MainWindow::on_go_back);
    go_menu->addAction(QStringLiteral("Forward"), this, &MainWindow::on_go_forward);
    go_menu->addAction(QStringLiteral("Parent"), this, &MainWindow::on_go_parent);
    go_menu->addAction(QStringLiteral("Parent in New Window"), this, &MainWindow::on_parent_new_window);
    go_menu->addAction(QStringLiteral("Home"), this, &MainWindow::on_go_home);

    bookmarks_menu_ = new HistoryMenu(QStringLiteral("&Bookmarks"), this);
    menuBar()->addMenu(bookmarks_menu_);
    connect(bookmarks_menu_, &QMenu::aboutToShow, this, &MainWindow::on_rebuild_bookmarks_menu);

    history_menu_ = new HistoryMenu(QStringLiteral("H&istory"), this);
    menuBar()->addMenu(history_menu_);
    connect(history_menu_, &QMenu::aboutToShow, this, &MainWindow::on_rebuild_history_menu);

    auto* help_menu = menuBar()->addMenu(QStringLiteral("&Help"));
    help_menu->addAction(QStringLiteral("Filter expression help"), this, [this] {
      QMessageBox::information(this, QStringLiteral("Filter help"),
                               QString::fromStdString(dirtoo::filter::filter_help_text()));
    });
    help_menu->addAction(QStringLiteral("About dirtoo"), this, &MainWindow::on_about);
  }


  {
    auto add_shortcut = [this](const QKeySequence& seq, auto slot) {
      auto* act = new QAction(this);
      act->setShortcut(seq);
      connect(act, &QAction::triggered, this, slot);
      addAction(act);
      return act;
    };
    add_shortcut(QKeySequence::Cut, &MainWindow::on_cut);
    add_shortcut(QKeySequence::Copy, &MainWindow::on_copy);
    add_shortcut(QKeySequence::Paste, &MainWindow::on_paste);
    add_shortcut(QKeySequence::Delete, &MainWindow::on_delete_selected);
    add_shortcut(QKeySequence::ZoomIn, &MainWindow::on_zoom_in);
    add_shortcut(QKeySequence::ZoomOut, &MainWindow::on_zoom_out);
    add_shortcut(QKeySequence(Qt::Key_F2), &MainWindow::on_rename_selected);
    add_shortcut(QKeySequence(Qt::Key_F5), &MainWindow::on_refresh);
    add_shortcut(QKeySequence(Qt::Key_Backspace), &MainWindow::on_go_parent);
    add_shortcut(QKeySequence(Qt::ALT | Qt::Key_Up), &MainWindow::on_go_parent);
    add_shortcut(QKeySequence(Qt::ALT | Qt::Key_Home), &MainWindow::on_go_home);
    add_shortcut(QKeySequence(Qt::ALT | Qt::Key_Left), &MainWindow::on_go_back);
    add_shortcut(QKeySequence(Qt::ALT | Qt::Key_Right), &MainWindow::on_go_forward);
    add_shortcut(QKeySequence(QStringLiteral("Ctrl+L")), &MainWindow::on_focus_location);
    add_shortcut(QKeySequence(Qt::Key_Escape), &MainWindow::on_clear_filter);
    add_shortcut(QKeySequence(QStringLiteral("Ctrl+D")), &MainWindow::on_toggle_bookmark);
    // Home: Alt+Home (Ctrl+Shift+H toggles hidden files)

    add_shortcut(QKeySequence(Qt::Key_F3), &MainWindow::on_properties);
  }

  auto* central = new QWidget(this);
  auto* layout = new QVBoxLayout(central);

  {
    auto* loc_host = new QWidget(central);
    auto* loc_layout = new QVBoxLayout(loc_host);
    loc_layout->setContentsMargins(0, 0, 0, 0);
    loc_layout->setSpacing(0);

    location_buttons_ = new LocationButtonBar(loc_host);
    connect(location_buttons_, &LocationButtonBar::location_activated, this,
            &MainWindow::on_breadcrumb_location);
    connect(location_buttons_, &LocationButtonBar::location_activated_new_window, this,
            &MainWindow::on_breadcrumb_location_new_window);
    connect(location_buttons_, &LocationButtonBar::edit_requested, this,
            &MainWindow::on_location_edit_requested);
    connect(location_buttons_, &LocationButtonBar::urls_dropped, this,
            &MainWindow::on_breadcrumb_drop);

    location_edit_ = new QLineEdit(loc_host);
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

    loc_layout->addWidget(location_buttons_);
    loc_layout->addWidget(location_edit_);
    layout->addWidget(loc_host);
    location_stack_host_ = loc_host;
  }

  filter_edit_ = new QLineEdit(central);
  filter_edit_->setPlaceholderText(QStringLiteral("Filter by name or glob (e.g. *.png)…"));
  filter_edit_->installEventFilter(this);
  connect(filter_edit_, &QLineEdit::textChanged, this, &MainWindow::on_filter_changed);
  layout->addWidget(filter_edit_);

  search_edit_ = new QLineEdit(central);
  search_edit_->setPlaceholderText(
      QStringLiteral("Recursive search (filter expression, Enter to run, Esc to close)…"));
  search_edit_->setVisible(false);
  search_edit_->installEventFilter(this);
  connect(search_edit_, &QLineEdit::returnPressed, this, &MainWindow::on_search_submitted);
  layout->addWidget(search_edit_);

  message_area_ = new MessageArea(central);
  layout->addWidget(message_area_);

  model_ = new FileListModel(this);
  model_->set_icon_detail_level(3);
  dirtoo::filter::MediaMetaCache::instance().open();
  model_->set_collection(&collection_);
  connect(model_, &FileListModel::urls_dropped, this, &MainWindow::on_urls_dropped);

  view_stack_ = new QStackedWidget(central);

  tree_view_ = new FileTreeView(view_stack_);
  tree_view_->setModel(model_);
  tree_view_->setRootIsDecorated(false);
  tree_view_->setUniformRowHeights(true);
  tree_view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  tree_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  tree_view_->setSortingEnabled(false);
  tree_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  tree_view_->setDragEnabled(true);
  tree_view_->setAcceptDrops(true);
  tree_view_->setDropIndicatorShown(true);
  tree_view_->setDragDropMode(QAbstractItemView::DragDrop);
  tree_view_->setDefaultDropAction(Qt::CopyAction);
  tree_view_->setIconSize(QSize(24, 24));
  tree_view_->header()->setStretchLastSection(true);
  tree_view_->setColumnWidth(0, 320);
  tree_view_->setColumnWidth(1, 100);
  tree_view_->setColumnWidth(2, 160);
  connect(tree_view_, &QTreeView::activated, this, &MainWindow::on_item_activated);
  tree_view_->viewport()->installEventFilter(this);
  connect(tree_view_, &QWidget::customContextMenuRequested, this, &MainWindow::on_context_menu);
  connect(tree_view_->header(), &QHeaderView::sectionClicked, this, &MainWindow::on_header_clicked);
  view_stack_->addWidget(tree_view_);

  icon_view_ = new FileListView(view_stack_);
  icon_view_->setModel(model_);
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
  icon_view_->setDropIndicatorShown(true);
  icon_view_->setDragDropMode(QAbstractItemView::DragDrop);
  icon_view_->setDefaultDropAction(Qt::CopyAction);
  connect(icon_view_, &QListView::activated, this, &MainWindow::on_item_activated);
  icon_view_->viewport()->installEventFilter(this);
  connect(icon_view_, &QWidget::customContextMenuRequested, this, &MainWindow::on_context_menu);
  view_stack_->addWidget(icon_view_);
  apply_icon_zoom();

    connect(tree_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
          &MainWindow::on_selection_changed);
  connect(icon_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
          &MainWindow::on_selection_changed);

  layout->addWidget(view_stack_, 1);

  status_label_ = new QLabel(central);
  layout->addWidget(status_label_);
  setCentralWidget(central);

  leap_widget_ = new LeapWidget(this);
  connect(leap_widget_, &LeapWidget::leap, this, &MainWindow::on_leap);

  connect(&watcher_, &watcher::DirectoryWatcher::directory_changed, this,
          &MainWindow::on_directory_changed);
  connect(&watcher_, &watcher::DirectoryWatcher::message, this, [this](const QString& msg) {
    status_label_->setText(msg);
  });

  connect(&thumbnailer_, &thumbnail::Thumbnailer::thumbnail_ready, this,
          &MainWindow::on_thumbnail_ready);
  connect(&thumbnailer_, &thumbnail::Thumbnailer::thumbnail_failed, this,
          &MainWindow::on_thumbnail_failed);

  connect(qApp->clipboard(), &QClipboard::dataChanged, this, &MainWindow::update_edit_actions);

  connect(&archive_manager_, &archive::ArchiveManager::extraction_started, this,
          [this](const fs::Location&) {
            status_label_->setText(QStringLiteral("Extracting archive…"));
          });
  connect(&archive_manager_, &archive::ArchiveManager::extraction_ready, this,
          &MainWindow::on_archive_ready);
  connect(&archive_manager_, &archive::ArchiveManager::extraction_failed, this,
          &MainWindow::on_archive_failed);

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
  if (transfer_worker_ != nullptr) {
    QMetaObject::invokeMethod(transfer_worker_, &TransferWorker::cancel, Qt::QueuedConnection);
  }
  transfer_thread_.quit();
  transfer_thread_.wait(5000);
  if (dir_load_thread_ != nullptr) {
    dir_load_thread_->quit();
    dir_load_thread_->wait(3000);
  }
  if (sort_thread_ != nullptr) {
    sort_thread_->quit();
    sort_thread_->wait(3000);
  }
}

QAbstractItemView* MainWindow::current_view() const
{
  if (view_mode_ == ViewMode::Detail) {
    return tree_view_;
  }
  return icon_view_;
}

void MainWindow::apply_icon_zoom()
{
  if (view_mode_ == ViewMode::SmallIcons) {
    // Compact list-like rows (Python SequenceMode-ish). Zoom steps map to row height.
    static constexpr int kSmall[] = {16, 24, 32, 48, 64, 96, 128};
    const int zi = std::clamp(zoom_index_, 0, static_cast<int>(std::size(kSmall)) - 1);
    const int size = kSmall[zi];
    icon_view_->setViewMode(QListView::ListMode);
    icon_view_->setFlow(QListView::TopToBottom);
    icon_view_->setWrapping(true);
    icon_view_->setResizeMode(QListView::Adjust);
    icon_view_->setUniformItemSizes(true);
    icon_view_->setIconSize(QSize(size, size));
    icon_view_->setSpacing(2);
    icon_view_->setGridSize(QSize()); // let list mode size from icon + text
    if (model_ != nullptr) {
      model_->set_icon_style(false); // single-line name; decoration from model
    }
    const int detail = std::max(16, size / 2);
    tree_view_->setIconSize(QSize(detail, detail));
    return;
  }

  const int size = kZoomLevels[std::clamp(zoom_index_, 0, static_cast<int>(std::size(kZoomLevels)) - 1)];
  if (view_mode_ == ViewMode::Icons) {
    icon_view_->setViewMode(QListView::IconMode);
    icon_view_->setFlow(QListView::LeftToRight);
    icon_view_->setWrapping(true);
    icon_view_->setResizeMode(QListView::Adjust);
    icon_view_->setUniformItemSizes(true);
  }
  icon_view_->setIconSize(QSize(size, size));
  const int text_rows = model_ != nullptr ? model_->icon_text_rows() : 1;
  // Caption lines under the icon (name / size / date). Use a generous line height so
  // multi-line text is not clipped by the grid cell.
  const int text_h = 6 + text_rows * 18;
  // Cell must be at least as wide as the thumbnail; prefer a bit of side margin for
  // long names and for media badges that sit on the icon edges.
  const int cell_w = std::max(size + 40, 96);
  const int cell_h = size + text_h + 16;
  icon_view_->setGridSize(QSize(cell_w, cell_h));
  icon_view_->setSpacing(8);
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
    status_label_->setText(QString::fromUtf8(labels[std::clamp(lvl, 0, 4)]));
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

void MainWindow::on_zoom_in()
{
  const int max_zi = (view_mode_ == ViewMode::SmallIcons)
                         ? 6
                         : static_cast<int>(std::size(kZoomLevels)) - 1;
  if (zoom_index_ < max_zi) {
    ++zoom_index_;
    apply_icon_zoom();
  }
}

void MainWindow::on_zoom_out()
{
  if (zoom_index_ > 0) {
    --zoom_index_;
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
  } else if (mode == ViewMode::SmallIcons) {
    if (model_ != nullptr) {
      model_->set_icon_style(false);
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
    }
    view_stack_->setCurrentWidget(icon_view_);
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

void MainWindow::open_location(const fs::Location& location, bool record_history)
{
  stop_search();
  search_active_ = false;
  search_results_.clear();
  if (search_edit_ != nullptr && search_edit_->isVisible()) {
    search_edit_->hide();
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

  if (record_history) {
    if (history_index_ >= 0 && history_index_ + 1 < static_cast<int>(history_.size())) {
      history_.erase(history_.begin() + history_index_ + 1, history_.end());
    }
    if (history_.empty() || history_.back().as_url() != location.as_url()) {
      history_.push_back(location);
      history_index_ = static_cast<int>(history_.size()) - 1;
    } else {
      history_index_ = static_cast<int>(history_.size()) - 1;
    }
    // Unique location history for the History menu (most recent last).
    location_history_unique_.erase(
        std::remove_if(location_history_unique_.begin(), location_history_unique_.end(),
                       [&](const fs::Location& loc) { return loc.as_url() == location.as_url(); }),
        location_history_unique_.end());
    location_history_unique_.push_back(location);
    if (location_history_unique_.size() > 40) {
      location_history_unique_.erase(location_history_unique_.begin());
    }
  }
  update_history_actions();

  if (location_.is_archive()) {
    watcher_.stop();
    pending_archive_location_ = location_;

    // Reuse index when navigating within the same archive file.
    if (archive_listing_ok_ && indexed_archive_path_ == location_.as_path()) {
      on_directory_changed();
    } else {
      archive_listing_ok_ = false;
      archive_entries_.clear();
      QApplication::setOverrideCursor(Qt::WaitCursor);
      auto listed = archive::list_archive_entries(location_.as_path());
      QApplication::restoreOverrideCursor();
      if (listed) {
        archive_entries_ = std::move(*listed);
        archive_listing_ok_ = true;
        indexed_archive_path_ = location_.as_path();
        status_label_->setText(QStringLiteral("Archive index: %1 entries")
                                   .arg(archive_entries_.size()));
        on_directory_changed();
      } else {
        indexed_archive_path_.clear();
        status_label_->setText(QStringLiteral("Listing failed (%1); extracting…")
                                   .arg(QString::fromStdString(listed.error())));
        if (archive_manager_.status(fs::Location::from_archive(location_.as_path(), {}))
            != archive::ExtractStatus::Ready) {
          QApplication::setOverrideCursor(Qt::WaitCursor);
        }
        archive_manager_.open(location_);
      }
    }
  } else {
    watcher_.set_location(location_);
    watcher_.start();
  }

  if (auto* view = current_view()) {
    view->setFocus(Qt::OtherFocusReason);
  }
}

void MainWindow::on_location_entered()
{
  try {
    open_location(fs::Location::from_human(location_edit_->text().toStdString()));
  } catch (const std::exception& ex) {
    status_label_->setText(QString::fromUtf8(ex.what()));
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
  if (history_index_ <= 0) {
    return;
  }
  --history_index_;
  open_location(history_[static_cast<std::size_t>(history_index_)], false);
}

void MainWindow::on_go_forward()
{
  if (history_index_ + 1 >= static_cast<int>(history_.size())) {
    return;
  }
  ++history_index_;
  open_location(history_[static_cast<std::size_t>(history_index_)], false);
}

void MainWindow::update_history_actions()
{
  if (back_act_) {
    back_act_->setEnabled(history_index_ > 0);
  }
  if (forward_act_) {
    forward_act_->setEnabled(history_index_ + 1 < static_cast<int>(history_.size()));
  }
}

void MainWindow::update_edit_actions()
{
  if (paste_act_) {
    paste_act_->setEnabled(!transfer_busy_
                           && clipboard_has_paths(QApplication::clipboard()->mimeData()));
  }
}

void MainWindow::on_directory_changed()
{
  if (search_active_) {
    // Keep recursive search results until the user navigates away or closes search.
    return;
  }
  thumbnailer_.cancel_all();
  model_->clear_thumbnails();
  filter::MediaMetaCache::instance().bump_generation();

  // In-memory archive index: apply on UI thread (no directory walk).
  if (location_.is_archive() && archive_listing_ok_) {
    auto items = archive::fileinfos_for_prefix(location_, archive_entries_);
    collection_.sorter().set_ascending(sort_ascending_);
    collection_.set_items(std::move(items));
    if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()) {
      collection_.set_name_filter(filter_edit_->text().toStdString());
    }
    refresh_list();
    request_thumbnails_for_visible();
    return;
  }

  fs::Location load_loc = location_;
  if (location_.is_archive()) {
    const auto resolved = archive_manager_.resolved_directory(location_);
    if (!resolved) {
      status_label_->setText(QStringLiteral("Archive not ready"));
      return;
    }
    load_loc = fs::Location::from_path(*resolved);
  }

  const quint64 gen = ++dir_load_generation_;
  if (status_label_ != nullptr) {
    status_label_->setText(QStringLiteral("Loading…"));
  }
  // Clear the view immediately so navigation feels responsive.
  collection_.clear();
  refresh_list();

  if (dir_load_worker_ == nullptr) {
    return;
  }
  const QString path = QString::fromStdString(load_loc.as_path().string());
  QMetaObject::invokeMethod(dir_load_worker_, "load", Qt::QueuedConnection,
                            Q_ARG(QString, path), Q_ARG(quint64, gen));
}

void MainWindow::on_directory_loaded(quint64 generation, std::vector<fs::FileInfo> items)
{
  if (generation != dir_load_generation_ || search_active_) {
    return;
  }
  // Mark paths that appeared since the last listing of this location (watcher refresh).
  if (model_ != nullptr) {
    model_->clear_new_marks();
    if (known_paths_location_ == location_ && !known_paths_.empty()) {
      for (const auto& fi : items) {
        const QString p = QString::fromStdString(fi.path().string());
        if (!known_paths_.contains(p)) {
          model_->mark_new(p);
        }
      }
    }
    known_paths_.clear();
    known_paths_location_ = location_;
    for (const auto& fi : items) {
      known_paths_.insert(QString::fromStdString(fi.path().string()));
    }
  }
  collection_.sorter().set_ascending(sort_ascending_);
  // Show entries ASAP; order refined by SortWorker.
  collection_.set_items_unsorted(std::move(items));
  if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()) {
    collection_.set_name_filter(filter_edit_->text().toStdString());
  }
  refresh_list();
  if (status_label_ != nullptr) {
    status_label_->setText(QStringLiteral("%1 items").arg(collection_.visible_items().size()));
  }
  request_async_sort();
  request_thumbnails_for_visible();
}

void MainWindow::request_async_sort()
{
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
  request_thumbnails_for_visible();
}

void MainWindow::on_directory_load_failed(quint64 generation, QString error)
{
  if (generation != dir_load_generation_) {
    return;
  }
  if (status_label_ != nullptr) {
    status_label_->setText(error);
  }
  if (message_area_ != nullptr) {
    message_area_->show_error(error);
  }
}

void MainWindow::request_thumbnails_for_visible()
{
  if (view_mode_ == ViewMode::Detail) {
    return;
  }
  // Debounce rapid refresh/scroll storms (generation-safe via singleShot capturing this).
  QTimer::singleShot(80, this, [this] {
    if (view_mode_ == ViewMode::Detail) {
      return;
    }
    QMimeDatabase mime_db;
    std::vector<fs::Location> locs;
    QStringList mimes;
    const auto& visible = collection_.visible_items();
    locs.reserve(visible.size());
    mimes.reserve(static_cast<int>(visible.size()));
    for (const auto& fi : visible) {
      if (fi.is_directory() || fi.is_synthetic() || location_.is_archive()) {
        continue;
      }
      const QString path = QString::fromStdString(fi.path().string());
      if (model_ != nullptr) {
        model_->set_thumbnail_pending(path);
      }
      locs.push_back(fi.location());
      const auto mt = mime_db.mimeTypeForFile(path);
      mimes.push_back(mt.name());
    }
    if (!locs.empty()) {
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
  model_->set_thumbnail(QString::fromStdString(location.as_path().string()), QIcon(pix));
}

void MainWindow::on_thumbnail_failed(const fs::Location& location, const QString& message)
{
  (void)message;
  if (model_ != nullptr) {
    model_->set_thumbnail_failed(QString::fromStdString(location.as_path().string()));
  }
}

void MainWindow::on_filter_changed(const QString& text)
{
  collection_.set_name_filter(text.toStdString());
  refresh_list();
  request_thumbnails_for_visible();
  if (!text.isEmpty() && !collection_.filter_parse_ok()) {
    if (message_area_ != nullptr) {
      message_area_->show_info(QStringLiteral("Filter parse issue — using substring fallback"));
    }
  }
}

void MainWindow::on_header_clicked(int section)
{
  const auto col = static_cast<SortColumn>(section);
  if (sort_column_ == col) {
    sort_ascending_ = !sort_ascending_;
  } else {
    sort_column_ = col;
    sort_ascending_ = true;
  }
  using dirtoo::collection::SortKey;
  SortKey key = SortKey::Name;
  switch (col) {
  case SortColumn::Name:
    key = SortKey::Name;
    break;
  case SortColumn::Size:
    key = SortKey::Size;
    break;
  case SortColumn::Modified:
    key = SortKey::Modified;
    break;
  case SortColumn::Type:
    key = SortKey::Type;
    break;
  }
  collection_.sorter().set_ascending(sort_ascending_);
  collection_.sorter().set_key(key);
  request_async_sort();
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
  auto* view = current_view();
  if (view == nullptr || view->selectionModel() == nullptr) {
    return {};
  }
  return model_->files_at(view->selectionModel()->selectedIndexes());
}

void MainWindow::on_context_menu(const QPoint& pos)
{
  auto* view = current_view();
  if (view == nullptr) {
    return;
  }
  QMenu menu(this);
  menu.addAction(QStringLiteral("Open"), this, [this, view] {
    if (view->selectionModel() == nullptr) {
      return;
    }
    const auto rows = view->selectionModel()->selectedIndexes();
    if (!rows.isEmpty()) {
      on_item_activated(rows.first());
    }
  });
  menu.addAction(QStringLiteral("Open in New Window"), this, [this, view] {
    if (view->selectionModel() == nullptr || model_ == nullptr) {
      return;
    }
    const auto rows = view->selectionModel()->selectedIndexes();
    for (const auto& idx : rows) {
      if (idx.column() != 0) {
        continue;
      }
      const auto* fi = model_->file_at(idx.row());
      if (fi == nullptr) {
        continue;
      }
      auto* win = new MainWindow();
      win->setAttribute(Qt::WA_DeleteOnClose);
      win->show();
      if (fi->is_directory() || fi->location().is_archive()) {
        win->open_location(fi->location());
      } else {
        win->open_location(fi->location().parent());
      }
      break; // one window from context menu
    }
  });
  menu.addAction(QStringLiteral("Open with…"), this, &MainWindow::on_open_with);
  menu.addAction(QStringLiteral("Open in Terminal"), this, &MainWindow::on_open_terminal);
  menu.addSeparator();
  menu.addAction(QStringLiteral("Cut"), this, &MainWindow::on_cut);
  menu.addAction(QStringLiteral("Copy"), this, &MainWindow::on_copy);
  menu.addAction(QStringLiteral("Paste"), this, &MainWindow::on_paste);
  menu.addAction(QStringLiteral("Copy Path"), this, [this] {
    const auto selected = selected_fileinfos();
    if (selected.empty()) {
      return;
    }
    QStringList paths;
    for (const auto& fi : selected) {
      paths << QString::fromStdString(fi.path().string());
    }
    QApplication::clipboard()->setText(paths.join(QLatin1Char('\n')));
    if (status_label_ != nullptr) {
      status_label_->setText(QStringLiteral("Copied %1 path(s)").arg(paths.size()));
    }
  });
  menu.addSeparator();
  menu.addAction(QStringLiteral("Rename…"), this, &MainWindow::on_rename_selected);
  menu.addAction(QStringLiteral("Delete…"), this, &MainWindow::on_delete_selected);
  menu.addAction(QStringLiteral("Properties…"), this, &MainWindow::on_properties);
  menu.addSeparator();
  menu.addAction(QStringLiteral("Reload Thumbnails"), this, &MainWindow::on_reload_thumbnails);
  menu.addAction(QStringLiteral("Prepare Thumbnails"), this, &MainWindow::on_prepare_thumbnails);
  menu.addSeparator();
  menu.addAction(QStringLiteral("New Folder…"), this, &MainWindow::on_mkdir);
  menu.exec(view->viewport()->mapToGlobal(pos));
}

void MainWindow::set_clipboard(ClipboardMode mode)
{
  const auto selected = selected_fileinfos();
  if (selected.empty()) {
    status_label_->setText(QStringLiteral("Nothing selected"));
    return;
  }
  std::vector<std::filesystem::path> paths;
  paths.reserve(selected.size());
  for (const auto& fi : selected) {
    paths.push_back(fi.path());
  }
  QApplication::clipboard()->setMimeData(make_clipboard_mime(mode, paths));
  status_label_->setText(QStringLiteral("%1 item(s) %2")
                             .arg(paths.size())
                             .arg(mode == ClipboardMode::Cut ? QStringLiteral("cut")
                                                             : QStringLiteral("copied")));
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
  if (transfer_busy_) {
    status_label_->setText(QStringLiteral("A transfer is already in progress"));
    return;
  }
  transfer_busy_ = true;
  last_transfer_mode_ = request.mode;
  update_edit_actions();

  if (transfer_dialog_ == nullptr) {
    transfer_dialog_ = new TransferDialog(this);
    const auto cancel_worker = [this] {
      if (transfer_worker_ != nullptr) {
        QMetaObject::invokeMethod(transfer_worker_, &TransferWorker::cancel, Qt::QueuedConnection);
      }
    };
    connect(transfer_dialog_, &TransferDialog::cancel_requested, this, cancel_worker);
    connect(transfer_dialog_, &QDialog::rejected, this, cancel_worker);
  }
  transfer_dialog_->reset();
  transfer_dialog_->set_title_text(request.mode == ClipboardMode::Cut ? QStringLiteral("Moving…")
                                                                     : QStringLiteral("Copying…"));
  transfer_dialog_->show();

  QMetaObject::invokeMethod(transfer_worker_, [this, request] { transfer_worker_->run(request); },
                            Qt::QueuedConnection);
}

void MainWindow::on_paste()
{
  if (location_.is_archive()) {
    status_label_->setText(QStringLiteral("Read-only: browsing inside an archive"));
    return;
  }

  if (transfer_busy_) {
    return;
  }

  const ClipboardPayload payload = parse_clipboard_mime(QApplication::clipboard()->mimeData());
  if (payload.paths.empty()) {
    status_label_->setText(QStringLiteral("Clipboard has no files"));
    return;
  }

  TransferRequest req;
  req.mode = payload.mode;
  req.destination_directory = location_.as_path();
  req.sources = payload.paths;
  start_transfer(req);
}

void MainWindow::on_transfer_item_started(int index, int total, const QString& path)
{
  if (transfer_dialog_ != nullptr) {
    transfer_dialog_->set_item_progress(index, total);
    transfer_dialog_->set_current_file(path);
  }
}

void MainWindow::on_transfer_byte_progress(quint64 done, quint64 total, const QString& path)
{
  if (transfer_dialog_ != nullptr) {
    transfer_dialog_->set_current_file(path);
    transfer_dialog_->set_progress(done, total);
  }
}

void MainWindow::on_transfer_conflict(const QString& destination_name)
{
  // Runs on UI thread (QueuedConnection from worker signal).
  const auto chosen = ask_conflict_policy(this, destination_name);
  if (transfer_worker_ == nullptr) {
    return;
  }
  if (!chosen) {
    QMetaObject::invokeMethod(
        transfer_worker_,
        [this] { transfer_worker_->resolve_conflict(dirops::ConflictPolicy::Fail, false); },
        Qt::QueuedConnection);
  } else {
    const auto policy = *chosen;
    QMetaObject::invokeMethod(
        transfer_worker_,
        [this, policy] { transfer_worker_->resolve_conflict(policy, true); },
        Qt::QueuedConnection);
  }
}

void MainWindow::on_transfer_finished(TransferSummary summary)
{
  transfer_busy_ = false;
  if (transfer_dialog_ != nullptr) {
    transfer_dialog_->hide();
  }

  if (!summary.error.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("Transfer"), summary.error);
  }

  if (last_transfer_mode_ == ClipboardMode::Cut && summary.completed > 0 && !summary.cancelled) {
    QApplication::clipboard()->clear();
  }

  if (summary.cancelled) {
    status_label_->setText(QStringLiteral("Transfer cancelled (%1 done, %2 skipped)")
                               .arg(summary.completed)
                               .arg(summary.skipped));
  } else {
    status_label_->setText(QStringLiteral("Transfer: %1 done, %2 skipped")
                               .arg(summary.completed)
                               .arg(summary.skipped));
  }

  on_directory_changed();
  update_edit_actions();
}

void MainWindow::on_mkdir()
{
  if (location_.is_archive()) {
    status_label_->setText(QStringLiteral("Read-only: browsing inside an archive"));
    return;
  }

  bool ok = false;
  const QString name = QInputDialog::getText(this, QStringLiteral("New Folder"),
                                             QStringLiteral("Folder name:"),
                                             QLineEdit::Normal, QStringLiteral("New Folder"), &ok);
  if (!ok || name.trimmed().isEmpty()) {
    return;
  }

  const auto dest = location_.as_path() / name.trimmed().toStdString();
  if (std::filesystem::exists(dest)) {
    const auto chosen = ask_conflict_policy(this, name.trimmed());
    if (!chosen || *chosen == dirops::ConflictPolicy::Skip) {
      return;
    }
    if (*chosen == dirops::ConflictPolicy::Overwrite) {
      auto rm = dirops::remove_path(dest);
      if (!rm) {
        QMessageBox::warning(this, QStringLiteral("New Folder"),
                             QString::fromStdString(rm.error().to_string()));
        return;
      }
    } else if (*chosen == dirops::ConflictPolicy::Rename) {
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
    QMessageBox::warning(this, QStringLiteral("New Folder"),
                         QString::fromStdString(result.error().to_string()));
    return;
  }
  on_directory_changed();
}

void MainWindow::on_rename_selected()
{
  if (location_.is_archive()) {
    status_label_->setText(QStringLiteral("Read-only: browsing inside an archive"));
    return;
  }

  const auto selected = selected_fileinfos();
  if (selected.size() != 1) {
    status_label_->setText(QStringLiteral("Select exactly one item to rename"));
    return;
  }

  const auto& fi = selected.front();
  bool ok = false;
  const QString name = QInputDialog::getText(this, QStringLiteral("Rename"),
                                             QStringLiteral("New name:"), QLineEdit::Normal,
                                             QString::fromStdString(fi.basename()), &ok);
  if (!ok || name.trimmed().isEmpty()) {
    return;
  }

  const auto dest = fi.path().parent_path() / name.trimmed().toStdString();
  dirops::Options opt;
  if (std::filesystem::exists(dest) && dest != fi.path()) {
    const auto chosen = ask_conflict_policy(this, name.trimmed());
    if (!chosen) {
      return;
    }
    opt.conflict = *chosen;
  }

  auto result = dirops::rename_path(fi.path(), dest, opt);
  if (!result) {
    QMessageBox::warning(this, QStringLiteral("Rename"),
                         QString::fromStdString(result.error().to_string()));
    return;
  }
  on_directory_changed();
}

void MainWindow::on_delete_selected()
{
  if (location_.is_archive()) {
    status_label_->setText(QStringLiteral("Read-only: browsing inside an archive"));
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
      QMessageBox::warning(this, QStringLiteral("Delete"),
                           QString::fromStdString(result.error().to_string()));
      break;
    }
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
    status_label_->setText(QStringLiteral("Nothing selected"));
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
  const auto selected = selected_fileinfos();
  const int total = model_->rowCount();
  if (selected.empty()) {
    status_label_->setText(QStringLiteral("%1 items in %2")
                               .arg(total)
                               .arg(QString::fromStdString(location_.as_path().string())));
    return;
  }

  std::uint64_t bytes = 0;
  int dirs = 0;
  for (const auto& fi : selected) {
    if (fi.is_directory()) {
      ++dirs;
    } else {
      bytes += fi.size();
    }
  }
  QString extra;
  if (selected.size() == 1) {
    extra = QString::fromStdString(selected.front().basename());
  } else {
    extra = QStringLiteral("%1 selected").arg(selected.size());
    if (bytes > 0) {
      extra += QStringLiteral(" (%1)").arg(QLocale::system().formattedDataSize(static_cast<qint64>(bytes)));
    }
    if (dirs > 0) {
      extra += QStringLiteral(", %1 folders").arg(dirs);
    }
  }
  status_label_->setText(QStringLiteral("%1 — %2 items in %3")
                             .arg(extra)
                             .arg(total)
                             .arg(QString::fromStdString(location_.as_path().string())));
}

void MainWindow::on_urls_dropped(const QList<QUrl>& urls, Qt::DropAction action)
{
  if (transfer_busy_ || urls.isEmpty()) {
    return;
  }

  TransferRequest req;
  req.mode = (action == Qt::MoveAction) ? ClipboardMode::Cut : ClipboardMode::Copy;
  req.destination_directory = location_.as_path();
  for (const QUrl& url : urls) {
    if (url.isLocalFile()) {
      req.sources.emplace_back(url.toLocalFile().toStdString());
    }
  }
  if (req.sources.empty()) {
    return;
  }

  // Avoid copying a directory into itself.
  std::vector<std::filesystem::path> filtered;
  for (const auto& src : req.sources) {
    const auto dest = req.destination_directory / src.filename();
    if (src == req.destination_directory || src == dest) {
      continue;
    }
    filtered.push_back(src);
  }
  req.sources = std::move(filtered);
  if (req.sources.empty()) {
    status_label_->setText(QStringLiteral("Drop ignored (invalid targets)"));
    return;
  }
  start_transfer(req);
}

void MainWindow::restore_settings()
{
  const AppSettings s = load_settings();
  if (s.zoom_index >= 0 && s.zoom_index < static_cast<int>(std::size(kZoomLevels))) {
    zoom_index_ = s.zoom_index;
    if (model_ != nullptr) {
      model_->set_icon_detail_level(s.icon_detail_level);
    model_->set_crop_thumbnails(s.crop_thumbnails);
    if (crop_thumbnails_act_ != nullptr) {
      crop_thumbnails_act_->setChecked(s.crop_thumbnails);
    }
    }
    apply_icon_zoom();
  }
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
  if (filter_edit_ != nullptr) {
    filter_edit_->setVisible(s.show_filter || s.filter_pinned);
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
}

void MainWindow::persist_settings() const
{
  AppSettings s;
  if (view_mode_ == ViewMode::Icons) {
    s.view_mode = QStringLiteral("icons");
  } else if (view_mode_ == ViewMode::SmallIcons) {
    s.view_mode = QStringLiteral("small");
  } else {
    s.view_mode = QStringLiteral("detail");
  }
  s.zoom_index = zoom_index_;
  if (model_ != nullptr) {
    s.icon_detail_level = model_->icon_detail_level();
    s.crop_thumbnails = model_->crop_thumbnails();
  }
  s.show_hidden = collection_.show_hidden();
  s.show_filter = show_filter_act_ != nullptr && show_filter_act_->isChecked();
  s.filter_pinned = filter_pinned_;
  s.window_geometry = saveGeometry();
  s.window_state = saveState();
  s.last_location = QString::fromStdString(location_.as_path().string());
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
  status_label_->setText(QStringLiteral("Refreshed"));
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
    status_label_->setText(QStringLiteral("Nothing selected"));
    return;
  }
  std::vector<std::filesystem::path> paths;
  for (const auto& fi : selected) {
    paths.push_back(fi.path());
  }
  if (!open_with_command_dialog(this, paths)) {
    return;
  }
}

void MainWindow::on_open_terminal()
{
  std::filesystem::path dir = location_.as_path();
  const auto selected = selected_fileinfos();
  if (selected.size() == 1 && selected.front().is_directory()) {
    dir = selected.front().path();
  }
  if (!open_in_terminal(dir)) {
    status_label_->setText(QStringLiteral("Could not launch a terminal emulator"));
  }
}


void MainWindow::on_toggle_hidden(bool checked)
{
  collection_.set_show_hidden(checked);
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
  search_edit_->setVisible(true);
  search_edit_->setFocus(Qt::ShortcutFocusReason);
  search_edit_->selectAll();
}

void MainWindow::stop_search()
{
  if (search_worker_ != nullptr) {
    search_worker_->cancel();
  }
  if (search_thread_ != nullptr) {
    search_thread_->quit();
    search_thread_->wait(3000);
    search_thread_->deleteLater();
    search_thread_ = nullptr;
    search_worker_ = nullptr;
  }
  search_active_ = false;
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
    status_label_->setText(QStringLiteral("Recursive search is not available inside archives"));
    return;
  }

  stop_search();
  search_results_.clear();
  search_active_ = true;
  collection_.clear();
  collection_.clear_filter();
  refresh_list();

  search_thread_ = new QThread(this);
  search_worker_ = new SearchWorker();
  search_worker_->moveToThread(search_thread_);

  connect(search_thread_, &QThread::finished, search_worker_, &QObject::deleteLater);
  connect(search_worker_, &SearchWorker::match_found, this, &MainWindow::on_search_match);
  connect(search_worker_, &SearchWorker::finished, this, &MainWindow::on_search_finished);
  connect(search_worker_, &SearchWorker::progress, this, &MainWindow::on_search_progress);

  const QString root = QString::fromStdString(location_.as_path().string());
  const bool show_hidden = show_hidden_act_ != nullptr && show_hidden_act_->isChecked();

  connect(search_thread_, &QThread::started, search_worker_,
          [this, root, expr, show_hidden] {
            search_worker_->start(root, expr, show_hidden, /*max_depth=*/-1);
          });

  status_label_->setText(QStringLiteral("Searching…"));
  if (message_area_ != nullptr) {
    message_area_->show_info(QStringLiteral("Recursive search: %1").arg(expr));
  }
  search_thread_->start();
}

void MainWindow::on_search_match(const QString& path, bool is_directory, quint64 size)
{
  if (!search_active_) {
    return;
  }
  auto info = fs::FileInfo::from_path(std::filesystem::path{path.toStdString()});
  if (info.basename().empty()) {
    info = fs::FileInfo::synthetic(fs::Location::from_path(std::filesystem::path{path.toStdString()}),
                                   std::filesystem::path{path.toStdString()}.filename().string(),
                                   is_directory, size);
  }
  search_results_.push_back(info);
  collection_.add(std::move(info));
  // Avoid full model reset every hit — refresh periodically.
  if (search_results_.size() % 32 == 1 || search_results_.size() < 8) {
    refresh_list();
  }
}

void MainWindow::on_search_progress(quint64 visited, quint64 matched)
{
  (void)visited;
  status_label_->setText(QStringLiteral("Searching… %1 matches").arg(matched));
}

void MainWindow::on_search_finished(quint64 matched, quint64 visited, const QString& error)
{
  refresh_list();
  request_thumbnails_for_visible();
  if (!error.isEmpty() && error != QStringLiteral("cancelled")) {
    status_label_->setText(error);
    if (message_area_ != nullptr) {
      message_area_->show_info(error);
    }
  } else if (error == QStringLiteral("cancelled")) {
    status_label_->setText(
        QStringLiteral("Search cancelled — %1 matches (%2 visited)").arg(matched).arg(visited));
  } else {
    status_label_->setText(
        QStringLiteral("Search done — %1 matches (%2 visited)").arg(matched).arg(visited));
  }
  if (search_thread_ != nullptr) {
    search_thread_->quit();
    search_thread_->wait(1000);
    search_thread_->deleteLater();
    search_thread_ = nullptr;
    search_worker_ = nullptr;
  }
  // Keep search_active_ true so directory watcher does not wipe results until user navigates.
}

void MainWindow::on_about()
{
  show_about_dialog(this);
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
  status_label_->setText(QStringLiteral("Archive ready — %1")
                             .arg(QString::fromStdString(archive_location.as_path().filename().string())));
  on_directory_changed();
}

void MainWindow::on_archive_failed(const fs::Location& archive_location, const QString& message)
{
  QApplication::restoreOverrideCursor();
  if (!location_.is_archive() || location_.as_path() != archive_location.as_path()) {
    return;
  }
  QMessageBox::warning(this, QStringLiteral("Archive"), message);
  status_label_->setText(message);
  // Fall back to parent directory so the user is not stuck on a failed archive view.
  open_location(fs::Location::from_path(archive_location.as_path().parent_path()), false);
}


void MainWindow::on_clear_filter()
{
  if (search_edit_ != nullptr && search_edit_->isVisible()) {
    stop_search();
    search_active_ = false;
    search_results_.clear();
    search_edit_->hide();
    search_edit_->clear();
    on_directory_changed();
    return;
  }
  if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()) {
    filter_edit_->clear();
    return;
  }
  if (filter_edit_ != nullptr && filter_edit_->isVisible() && !filter_pinned_
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
    status_label_->setText(QStringLiteral("Cannot drop into an archive (read-only)"));
    return;
  }
  if (transfer_busy_ || urls.isEmpty()) {
    return;
  }
  TransferRequest req;
  req.mode = (action == Qt::MoveAction) ? ClipboardMode::Cut : ClipboardMode::Copy;
  req.destination_directory = target.as_path();
  for (const QUrl& url : urls) {
    if (url.isLocalFile()) {
      req.sources.emplace_back(url.toLocalFile().toStdString());
    }
  }
  if (req.sources.empty()) {
    return;
  }
  start_transfer(req);
}


bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
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

  if (obj == search_edit_ && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Escape) {
      stop_search();
      search_active_ = false;
      search_results_.clear();
      search_edit_->hide();
      search_edit_->clear();
      on_directory_changed();
      return true;
    }
  }

  if (obj == filter_edit_ && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Up) {
      if (!filter_history_.isEmpty()) {
        if (filter_history_index_ < 0) {
          filter_history_index_ = filter_history_.size() - 1;
        } else if (filter_history_index_ > 0) {
          --filter_history_index_;
        }
        filter_edit_->setText(filter_history_.at(filter_history_index_));
      }
      return true;
    }
    if (ke->key() == Qt::Key_Down) {
      if (!filter_history_.isEmpty() && filter_history_index_ >= 0) {
        if (filter_history_index_ + 1 < filter_history_.size()) {
          ++filter_history_index_;
          filter_edit_->setText(filter_history_.at(filter_history_index_));
        } else {
          filter_history_index_ = -1;
          filter_edit_->clear();
        }
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

  auto* view = current_view();
  int start = 0;
  if (view != nullptr && view->selectionModel() != nullptr) {
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

  if (found < 0 || view == nullptr) {
    return;
  }
  const QModelIndex idx = model_->index(found, 0);
  view->setCurrentIndex(idx);
  view->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  view->scrollTo(idx);
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
  history_menu_->addAction(QStringLiteral("Back"), this, &MainWindow::on_go_back);
  history_menu_->addAction(QStringLiteral("Forward"), this, &MainWindow::on_go_forward);
  history_menu_->addSeparator();

  // Most recent first.
  int count = 0;
  for (auto it = location_history_unique_.rbegin();
       it != location_history_unique_.rend() && count < 35; ++it, ++count) {
    const fs::Location loc = *it;
    QString label = loc.is_archive() ? QString::fromStdString(loc.as_url())
                                     : QString::fromStdString(loc.as_path().string());
    auto* act = history_menu_->addAction(label);
    connect(act, &QAction::triggered, this, [this, loc] {
      if (history_menu_ != nullptr && history_menu_->middle_pressed()) {
        open_new_window(loc);
      } else if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
        open_new_window(loc);
      } else {
        open_location(loc);
      }
    });
  }
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
    if (status_label_ != nullptr) {
      status_label_->setText(QStringLiteral("Reloading thumbnails for visible items…"));
    }
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
  if (status_label_ != nullptr) {
    status_label_->setText(QStringLiteral("Reloading %1 thumbnail(s)…").arg(locs.size()));
  }
}

void MainWindow::on_prepare_thumbnails()
{
  if (view_mode_ == ViewMode::Detail) {
    // Still useful: switch-less prepare for when user opens icons next
  }
  request_thumbnails_for_visible();
  if (status_label_ != nullptr) {
    status_label_->setText(QStringLiteral("Preparing thumbnails for visible items…"));
  }
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
  s.zoom_index = zoom_index_;
  if (model_ != nullptr) {
    s.icon_detail_level = model_->icon_detail_level();
    s.crop_thumbnails = model_->crop_thumbnails();
  }
  s.show_hidden = collection_.show_hidden();
  s.show_filter = show_filter_act_ != nullptr && show_filter_act_->isChecked();
  s.filter_pinned = filter_pinned_;
  if (!show_preferences_dialog(this, &s)) {
    return;
  }
  save_settings(s);
  apply_settings(s);
}

void MainWindow::apply_settings(const AppSettings& s)
{
  if (model_ != nullptr) {
    model_->set_icon_detail_level(s.icon_detail_level);
    model_->set_crop_thumbnails(s.crop_thumbnails);
    if (crop_thumbnails_act_ != nullptr) {
      crop_thumbnails_act_->setChecked(s.crop_thumbnails);
    }
  }
  if (s.zoom_index >= 0 && s.zoom_index < static_cast<int>(std::size(kZoomLevels))) {
    zoom_index_ = s.zoom_index;
    apply_icon_zoom();
  }
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
  filter_pinned_ = s.filter_pinned;
  if (pin_filter_act_ != nullptr) {
    pin_filter_act_->setChecked(s.filter_pinned);
  }
  if (show_filter_act_ != nullptr) {
    show_filter_act_->setChecked(s.show_filter || s.filter_pinned);
  }
  if (filter_edit_ != nullptr) {
    filter_edit_->setVisible(s.show_filter || s.filter_pinned);
  }
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
  if (status_label_ != nullptr) {
    status_label_->setText(now ? QStringLiteral("Bookmarked") : QStringLiteral("Bookmark removed"));
  }
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
    bookmarks_menu_->addAction(QStringLiteral("Remove Bookmark for This Location"), this,
                               &MainWindow::on_toggle_bookmark);
  } else {
    bookmarks_menu_->addAction(QStringLiteral("Bookmark This Location"), this,
                               &MainWindow::on_toggle_bookmark);
  }
  bookmarks_menu_->addSeparator();

  const auto entries = bookmarks_.entries();
  if (entries.empty()) {
    auto* act = bookmarks_menu_->addAction(QStringLiteral("(no bookmarks yet)"));
    act->setEnabled(false);
    return;
  }

  for (const auto& loc : entries) {
    QString label = loc.is_archive() ? QString::fromStdString(loc.as_url())
                                     : QString::fromStdString(loc.as_path().string());
    auto* act = bookmarks_menu_->addAction(label);
    connect(act, &QAction::triggered, this, [this, loc] {
      if (bookmarks_menu_ != nullptr && bookmarks_menu_->middle_pressed()) {
        open_new_window(loc);
      } else if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
        open_new_window(loc);
      } else {
        open_location(loc);
      }
    });
  }
}

} // namespace dirtoo::app

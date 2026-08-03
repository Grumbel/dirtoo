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
#include "message_area.hpp"
#include "leap_widget.hpp"
#include "location_button_bar.hpp"
#include "path_completion_worker.hpp"
#include "directory_load_worker.hpp"
#include "sort_worker.hpp"
#include "filter_worker.hpp"
#include "directory_thumbnail_worker.hpp"
#include "graphics_file_view.hpp"
#include "file_list_model.hpp"

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
#include <QPushButton>
#include <QTimer>
#include <QAction>
#include <QListView>
#include <QWidget>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QThread>

namespace dirtoo::app {

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

  read_only_act_ = toolbar->addAction(theme_icon("object-locked", "changes-prevent"),
                                      QStringLiteral("Read-only"));
  read_only_act_->setCheckable(true);
  read_only_act_->setToolTip(QStringLiteral("Block all filesystem modifications"));
  read_only_act_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
  connect(read_only_act_, &QAction::toggled, this, &MainWindow::on_toggle_read_only);
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

} // namespace dirtoo::app

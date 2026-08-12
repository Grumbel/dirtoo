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

  sidebar_.places().ensure_model();
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
  sidebar_tree_->setModel(sidebar_.places().model());
  sidebar_tree_->setHeaderHidden(true);
  sidebar_tree_->setUniformRowHeights(true);
  sidebar_tree_->setAnimated(true);
  sidebar_tree_->setExpandsOnDoubleClick(true);
  sidebar_tree_->setFrameShape(QFrame::NoFrame);
  sidebar_tree_->setSelectionMode(QAbstractItemView::SingleSelection);
  connect(sidebar_tree_, &QTreeView::activated, this, &MainWindow::on_sidebar_activated);
  connect(sidebar_tree_, &QTreeView::clicked, this, &MainWindow::on_sidebar_activated);
  places_layout->addWidget(sidebar_tree_, 1);

  sidebar_.bind(sidebar_widget_, sidebar_tree_, main_splitter_);
  sidebar_.set_open_path_handler([this](const QString& path) {
    open_location(fs::Location::from_path(std::filesystem::path(path.toStdString())), true);
  });

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
  location_stack_host_ = location_chrome_.create_bar(central);
  location_chrome_.setup_completion();
  connect(&location_chrome_, &LocationChrome::location_activated, this,
          &MainWindow::on_breadcrumb_location);
  connect(&location_chrome_, &LocationChrome::location_activated_new_window, this,
          &MainWindow::on_breadcrumb_location_new_window);
  connect(&location_chrome_, &LocationChrome::path_entered, this, &MainWindow::on_location_entered);
  connect(&location_chrome_, &LocationChrome::urls_dropped, this, &MainWindow::on_breadcrumb_drop);
  layout->addWidget(location_stack_host_);
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
  connect(graphics_view_, &GraphicsFileView::visible_window_changed, this,
        [this] { request_thumbnails_for_visible(); });
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


} // namespace dirtoo::app

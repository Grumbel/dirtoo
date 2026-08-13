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

  // Left: directory tree sidebar (chrome owned by SidebarController).
  auto* sidebar_host = sidebar_.create(main_splitter_);
  rebuild_sidebar_places();
  sidebar_.set_open_path_handler([this](const QString& path) {
    open_location(fs::Location::from_path(std::filesystem::path(path.toStdString())), true);
  });
  devices_.attach(sidebar_.devices_list(), this);
  connect(&devices_, &DevicesController::open_path, this, [this](const QString& path) {
    open_location(fs::Location::from_path(std::filesystem::path(path.toStdString())), true);
  });
  connect(&devices_, &DevicesController::status_message, this, &MainWindow::set_status);
  devices_.refresh();

  main_splitter_->addWidget(sidebar_host);

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

  // Search row: chrome owned by FilterSearchChrome (same expression language + Help).
  {
  auto* search_row = filter_search_.create_search_row(central);
  if (auto* edit = filter_search_.search_edit()) {
    edit->installEventFilter(this);
  }
  connect(&filter_search_, &FilterSearchChrome::search_submitted, this,
          &MainWindow::on_search_submitted);
  connect(&filter_search_, &FilterSearchChrome::help_requested, this,
          &MainWindow::on_show_filter_help);
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
  {
    auto* del = new FileItemDelegate(model_, tree_view_);
    tree_view_->setItemDelegate(del);
    connect(del, &FileItemDelegate::tag_chip_clicked, this, [this](const QString& tag) {
      const QString expr = QStringLiteral("tag:%1").arg(tag);
      if (filter_search_.filter_text() == expr) {
        return;
      }
      filter_search_.set_filter_text(expr);
      filter_search_.set_filter_visible(true);
    });
  }
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
  {
    auto* del = new FileItemDelegate(model_, icon_view_);
    icon_view_->setItemDelegate(del);
    connect(del, &FileItemDelegate::tag_chip_clicked, this, [this](const QString& tag) {
      const QString expr = QStringLiteral("tag:%1").arg(tag);
      if (filter_search_.filter_text() == expr) {
        return;
      }
      filter_search_.set_filter_text(expr);
      filter_search_.set_filter_visible(true);
    });
  }
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
  connect(graphics_view_, &GraphicsFileView::tag_chip_clicked, this, [this](const QString& tag) {
    // Prefer namespaced form for an exact match; unnamespaced local names still work
    // via tag: predicate (namespace ignored unless explicit).
    const QString expr = QStringLiteral("tag:%1").arg(tag);
    if (filter_search_.filter_text() == expr) {
      return;
    }
    filter_search_.set_filter_text(expr);
    filter_search_.set_filter_visible(true);
  });
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

  quick_filter_bar_ = new QuickFilterBar(central);
  connect(quick_filter_bar_, &QuickFilterBar::filter_requested, this, [this](const QString& expr) {
    filter_search_.set_filter_visible(true);
    filter_search_.set_filter_text(expr);
  });
  connect(quick_filter_bar_, &QuickFilterBar::pin_current_requested, this, [this] {
    const QString expr = filter_search_.filter_text().trimmed();
    if (expr.isEmpty() || quick_filter_bar_ == nullptr) {
      return;
    }
    quick_filter_bar_->pin_expression(expr);
  });
  layout->addWidget(quick_filter_bar_);

  // Filter row at bottom (parity with dirtoo-py BottomToolBarArea filter toolbar).
  {
  auto* filter_row = filter_search_.create_filter_row(central);
  if (auto* edit = filter_search_.filter_edit()) {
    edit->installEventFilter(this);
  }
  connect(&filter_search_, &FilterSearchChrome::filter_text_changed, this,
          &MainWindow::on_filter_changed);
  // help_requested already connected from search row setup (same signal).
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

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"

#include "clipboard.hpp"
#include "about_dialog.hpp"
#include "app_settings.hpp"
#include "conflict_dialog.hpp"
#include "open_with.hpp"
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
#include <QEvent>
#include <QFileSystemModel>
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

  auto* toolbar = addToolBar(QStringLiteral("Main"));
  toolbar->setMovable(false);

  back_act_ = toolbar->addAction(QStringLiteral("Back"), this, &MainWindow::on_go_back);
  forward_act_ = toolbar->addAction(QStringLiteral("Forward"), this, &MainWindow::on_go_forward);
  parent_act_ = toolbar->addAction(QStringLiteral("Parent"), this, &MainWindow::on_go_parent);
  // Middle-click Parent → open parent in a new window.
  if (auto* btn = qobject_cast<QToolButton*>(toolbar->widgetForAction(parent_act_))) {
    btn->installEventFilter(this);
  }
  toolbar->addAction(QStringLiteral("Home"), this, &MainWindow::on_go_home);
  toolbar->addSeparator();
  toolbar->addAction(QStringLiteral("New Folder"), this, &MainWindow::on_mkdir);
  toolbar->addSeparator();
  toolbar->addAction(QStringLiteral("Cut"), this, &MainWindow::on_cut);
  toolbar->addAction(QStringLiteral("Copy"), this, &MainWindow::on_copy);
  paste_act_ = toolbar->addAction(QStringLiteral("Paste"), this, &MainWindow::on_paste);
  toolbar->addSeparator();

  auto* view_group = new QActionGroup(this);
  detail_act_ = toolbar->addAction(QStringLiteral("Detail"));
  icons_act_ = toolbar->addAction(QStringLiteral("Icons"));
  detail_act_->setCheckable(true);
  icons_act_->setCheckable(true);
  detail_act_->setChecked(true);
  view_group->addAction(detail_act_);
  view_group->addAction(icons_act_);
  connect(detail_act_, &QAction::triggered, this, &MainWindow::on_view_detail);
  connect(icons_act_, &QAction::triggered, this, &MainWindow::on_view_icons);

  toolbar->addSeparator();
  toolbar->addAction(QStringLiteral("Zoom −"), this, &MainWindow::on_zoom_out);
  toolbar->addAction(QStringLiteral("Zoom +"), this, &MainWindow::on_zoom_in);

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

    auto* view_menu = menuBar()->addMenu(QStringLiteral("&View"));
    view_menu->addAction(detail_act_);
    view_menu->addAction(icons_act_);
    view_menu->addSeparator();
    show_hidden_act_ = view_menu->addAction(QStringLiteral("Show Hidden Files"));
    show_hidden_act_->setCheckable(true);
    show_hidden_act_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+H")));
    connect(show_hidden_act_, &QAction::toggled, this, &MainWindow::on_toggle_hidden);
    view_menu->addSeparator();
    {
      auto* act = show_filter_act_ = view_menu->addAction(QStringLiteral("Show Filter"));
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
    {
      auto* act = view_menu->addAction(QStringLiteral("Jump to…"), this, &MainWindow::on_show_leap);
      act->setShortcut(QKeySequence(QStringLiteral("/")));
    }

      act->setShortcut(QKeySequence(Qt::Key_F5));
    }
    {
      auto* act = view_menu->addAction(QStringLiteral("Zoom In"), this, &MainWindow::on_zoom_in);
      act->setShortcut(QKeySequence::ZoomIn);
    }
    {
      auto* act = view_menu->addAction(QStringLiteral("Zoom Out"), this, &MainWindow::on_zoom_out);
      act->setShortcut(QKeySequence::ZoomOut);
    }

    auto* go_menu = menuBar()->addMenu(QStringLiteral("&Go"));
    go_menu->addAction(QStringLiteral("Back"), this, &MainWindow::on_go_back);
    go_menu->addAction(QStringLiteral("Forward"), this, &MainWindow::on_go_forward);
    go_menu->addAction(QStringLiteral("Parent"), this, &MainWindow::on_go_parent);
    go_menu->addAction(QStringLiteral("Parent in New Window"), this, &MainWindow::on_parent_new_window);
    go_menu->addAction(QStringLiteral("Home"), this, &MainWindow::on_go_home);

    history_menu_ = menuBar()->addMenu(QStringLiteral("H&istory"));
    connect(history_menu_, &QMenu::aboutToShow, this, &MainWindow::on_rebuild_history_menu);

    auto* help_menu = menuBar()->addMenu(QStringLiteral("&Help"));
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
      auto* fs_model = new QFileSystemModel(location_edit_);
      fs_model->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);
      fs_model->setRootPath(QStringLiteral("/"));
      auto* completer = new QCompleter(fs_model, location_edit_);
      completer->setCaseSensitivity(Qt::CaseInsensitive);
      completer->setCompletionMode(QCompleter::PopupCompletion);
      location_edit_->setCompleter(completer);
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

  model_ = new FileListModel(this);
  model_->set_collection(&collection_);
  connect(model_, &FileListModel::urls_dropped, this, &MainWindow::on_urls_dropped);

  view_stack_ = new QStackedWidget(central);

  tree_view_ = new QTreeView(view_stack_);
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

  icon_view_ = new QListView(view_stack_);
  icon_view_->setModel(model_);
  icon_view_->setViewMode(QListView::IconMode);
  icon_view_->setResizeMode(QListView::Adjust);
  icon_view_->setMovement(QListView::Static);
  icon_view_->setUniformItemSizes(true);
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
  if (transfer_worker_ != nullptr) {
    QMetaObject::invokeMethod(transfer_worker_, &TransferWorker::cancel, Qt::QueuedConnection);
  }
  transfer_thread_.quit();
  transfer_thread_.wait(5000);
}

QAbstractItemView* MainWindow::current_view() const
{
  return view_mode_ == ViewMode::Detail ? static_cast<QAbstractItemView*>(tree_view_)
                                        : static_cast<QAbstractItemView*>(icon_view_);
}

void MainWindow::apply_icon_zoom()
{
  const int size = kZoomLevels[zoom_index_];
  icon_view_->setIconSize(QSize(size, size));
  icon_view_->setGridSize(QSize(size + 28, size + 48));
  // Detail view keeps small icons; slightly scale with zoom for consistency.
  const int detail = std::max(16, size / 4);
  tree_view_->setIconSize(QSize(detail, detail));
}

void MainWindow::on_zoom_in()
{
  if (zoom_index_ + 1 < static_cast<int>(std::size(kZoomLevels))) {
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
    view_stack_->setCurrentWidget(tree_view_);
    detail_act_->setChecked(true);
  } else {
    view_stack_->setCurrentWidget(icon_view_);
    icons_act_->setChecked(true);
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

void MainWindow::open_location(const fs::Location& location, bool record_history)
{
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
  thumbnailer_.cancel_all();
  model_->clear_thumbnails();
  std::vector<fs::FileInfo> items;
  if (location_.is_archive()) {
    if (archive_listing_ok_) {
      items = archive::fileinfos_for_prefix(location_, archive_entries_);
    } else {
      const auto resolved = archive_manager_.resolved_directory(location_);
      if (!resolved) {
        status_label_->setText(QStringLiteral("Archive not ready"));
        return;
      }
      items = fs::list_directory(fs::Location::from_path(*resolved));
    }
  } else {
    items = fs::list_directory(location_);
  }
  collection_.set_items(std::move(items));

  switch (sort_column_) {
  case SortColumn::Name:
    collection_.sort_by_name(sort_ascending_);
    break;
  case SortColumn::Size:
    collection_.sort_by_size(sort_ascending_);
    break;
  case SortColumn::Modified:
    collection_.sort_by_mtime(sort_ascending_);
    break;
  case SortColumn::Type:
    collection_.sort_by_name(sort_ascending_);
    break;
  }

  if (!filter_edit_->text().isEmpty()) {
    collection_.set_name_filter(filter_edit_->text().toStdString());
  }
  refresh_list();
  request_thumbnails_for_visible();
}

void MainWindow::request_thumbnails_for_visible()
{
  if (view_mode_ != ViewMode::Icons) {
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
    locs.push_back(fi.location());
    const auto mt = mime_db.mimeTypeForFile(QString::fromStdString(fi.path().string()));
    mimes.push_back(mt.name());
  }
  thumbnailer_.request_many(locs, mimes, QStringLiteral("large"));
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
  (void)location;
  (void)message;
}

void MainWindow::on_filter_changed(const QString& text)
{
  collection_.set_name_filter(text.toStdString());
  refresh_list();
  request_thumbnails_for_visible();
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
  on_directory_changed();
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
  QMenu menu(this);
  menu.addAction(QStringLiteral("Open"), this, [this, view] {
    if (view == nullptr || view->selectionModel() == nullptr) {
      return;
    }
    const auto rows = view->selectionModel()->selectedIndexes();
    if (!rows.isEmpty()) {
      on_item_activated(rows.first());
    }
  });
  menu.addAction(QStringLiteral("Open with…"), this, &MainWindow::on_open_with);
  menu.addAction(QStringLiteral("Open in Terminal"), this, &MainWindow::on_open_terminal);
  menu.addSeparator();
  menu.addAction(QStringLiteral("Cut"), this, &MainWindow::on_cut);
  menu.addAction(QStringLiteral("Copy"), this, &MainWindow::on_copy);
  menu.addAction(QStringLiteral("Paste"), this, &MainWindow::on_paste);
  menu.addSeparator();
  menu.addAction(QStringLiteral("Rename…"), this, &MainWindow::on_rename_selected);
  menu.addAction(QStringLiteral("Delete…"), this, &MainWindow::on_delete_selected);
  menu.addAction(QStringLiteral("Properties…"), this, &MainWindow::on_properties);
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
    apply_icon_zoom();
  }
  collection_.set_show_hidden(s.show_hidden);
  if (show_hidden_act_ != nullptr) {
    show_hidden_act_->setChecked(s.show_hidden);
  }
  if (s.view_mode == QLatin1String("icons")) {
    set_view_mode(ViewMode::Icons);
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
  s.view_mode = (view_mode_ == ViewMode::Icons) ? QStringLiteral("icons") : QStringLiteral("detail");
  s.zoom_index = zoom_index_;
  s.show_hidden = collection_.show_hidden();
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
  if (filter_edit_ != nullptr && !filter_edit_->text().isEmpty()) {
    filter_edit_->clear();
    return;
  }
  // Second Escape: leave location line-edit mode if active.
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
    connect(act, &QAction::triggered, this, [this, loc] { open_location(loc); });
    // Middle-click via custom? QAction doesn't get middle easily; use context - skip for now
  }
}

} // namespace dirtoo::app

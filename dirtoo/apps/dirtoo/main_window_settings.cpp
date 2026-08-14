// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"
#include <QColor>
#include "tag_manager_dialog.hpp"
#include "tag_paint.hpp"
#include "view_zoom.hpp"

#include "app_settings.hpp"
#include "sort_settings.hpp"
#include "preferences_dialog.hpp"
#include "checksum_dialog.hpp"

#include "dirtoo/fs/location.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/tags/tag_store.hpp"
#include "activity_monitor.hpp"

#include <QInputDialog>
#include <QProgressDialog>
#include <QFileInfo>
#include <QApplication>
#include <QMessageBox>
#include <QStatusBar>
#include <QLineEdit>
#include "size_format.hpp"
#include <QHeaderView>
#include <filesystem>
#include <algorithm>
#include <set>

namespace dirtoo::app {

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
  zoom_.icons = s.zoom_icons;
  zoom_.list = s.zoom_list;
  zoom_.detail = s.zoom_detail;
  zoom_.clamp_all();
  icon_spacing_ = std::clamp(s.icon_spacing, 0, 48);
  icon_cell_padding_ = std::clamp(s.icon_cell_padding, 0, 240);
  if (!s.detail_columns.isEmpty()) {
    detail_columns_ = s.detail_columns;
  }
  apply_detail_column_visibility();
  apply_icon_zoom();
  collection_.set_show_hidden(s.show_hidden);
  if (show_hidden_act_ != nullptr) {
    show_hidden_act_->setChecked(s.show_hidden);
  }
  if (show_opened_state_act_ != nullptr) {
    show_opened_state_act_->setChecked(s.show_opened_state);
  }
  if (model_ != nullptr) {
    model_->set_show_opened_state(s.show_opened_state);
    model_->set_ui_colors(s.colors);
  }
  filter_pinned_ = s.filter_pinned;
  if (pin_filter_act_ != nullptr) {
    pin_filter_act_->setChecked(s.filter_pinned);
  }
  if (show_filter_act_ != nullptr) {
    show_filter_act_->setChecked(s.show_filter || s.filter_pinned);
  }
  // Toggle the whole filter row; keep the line edit itself always visible/enabled.
  filter_search_.set_filter_visible(s.show_filter || s.filter_pinned);
  if (auto* edit = filter_search_.filter_edit()) {
    edit->setVisible(true);
    edit->setEnabled(true);
  }
  collection_.sorter().set_directories_first(s.directories_first);
  {
    const collection::SortKey sk = sort_key_from_settings_string(s.sort_key);
    sort_key_ = sk;
    sort_ascending_ = s.sort_ascending;
    collection_.sorter().set_key(sk);
    collection_.sorter().set_ascending(s.sort_ascending);
    update_sort_toolbar_label();
  }
  {
    collection::GroupMode gm = collection::GroupMode::None;
    const QString g = s.group_mode.toLower();
    if (g == QLatin1String("day")) {
      gm = collection::GroupMode::Day;
    } else if (g == QLatin1String("directory")) {
      gm = collection::GroupMode::Directory;
    } else if (g == QLatin1String("duration")) {
      gm = collection::GroupMode::Duration;
    } else if (g == QLatin1String("session")) {
      gm = collection::GroupMode::Session;
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
      } else if (gm == collection::GroupMode::Session) {
        group_toolbar_btn_->setText(QStringLiteral("Session"));
      } else {
        group_toolbar_btn_->setText(QStringLiteral("None"));
      }
    }
  }
  if (s.view_mode == QLatin1String("icons")) {
    set_view_mode(ViewMode::Icons);
  } else if (s.view_mode == QLatin1String("relative")
             || s.view_mode == QLatin1String("relativeicons")) {
    set_view_mode(ViewMode::RelativeIcons);
  } else if (s.view_mode == QLatin1String("small") || s.view_mode == QLatin1String("smallicons")) {
    set_view_mode(ViewMode::List);
  } else {
    set_view_mode(ViewMode::Detail);
  }
  // Directory tree sidebar visibility + width (persisted as ui/show_sidebar,
  // ui/sidebar_width). Apply before restoreGeometry so the splitter layout is
  // consistent with the saved window size.
  if (show_sidebar_act_ != nullptr) {
    show_sidebar_act_->setChecked(s.show_sidebar);
  }
  if (sidebar_.widget() != nullptr) {
    sidebar_.set_visible(s.show_sidebar);
  }
  if (main_splitter_ != nullptr && s.sidebar_width > 40) {
    QList<int> sizes = main_splitter_->sizes();
    if (sizes.size() >= 2) {
      sizes[0] = s.sidebar_width;
      main_splitter_->setSizes(sizes);
    }
  }
  if (sidebar_.places().model() != nullptr) {
    sidebar_.set_show_hidden(s.show_hidden);
  }
  read_only_ = s.read_only;
  if (read_only_act_ != nullptr) {
    read_only_act_->setChecked(s.read_only);
  }
  update_mutation_actions();
  if (!s.window_geometry.isEmpty()) {
    restoreGeometry(s.window_geometry);
  }
  if (!s.window_state.isEmpty()) {
    restoreState(s.window_state);
  }
  // Restore persistent location history for the History menu.
  // Use from_human so file://…//archive URLs and bare …zip//archive: forms both
  // round-trip; from_path alone would drop the archive payload.
  std::vector<fs::Location> unique;
  for (const QString& entry : s.location_history) {
    if (entry.isEmpty()) {
      continue;
    }
    try {
      unique.push_back(fs::Location::from_human(entry.toStdString()));
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
  } else if (view_mode_ == ViewMode::RelativeIcons) {
    s.view_mode = QStringLiteral("relative");
  } else if (view_mode_ == ViewMode::List) {
    s.view_mode = QStringLiteral("small");
  } else {
    s.view_mode = QStringLiteral("detail");
  }
  s.zoom_icons = zoom_.icons;
  s.zoom_list = zoom_.list;
  s.zoom_detail = zoom_.detail;
  s.zoom_index = zoom_.icons;
  s.icon_spacing = icon_spacing_;
  s.icon_cell_padding = icon_cell_padding_;
  s.detail_columns = detail_columns_;
  if (model_ != nullptr) {
    s.icon_detail_level = model_->icon_detail_level();
    s.crop_thumbnails = model_->crop_thumbnails();
  }
  s.show_hidden = collection_.show_hidden();
  s.show_opened_state =
      show_opened_state_act_ != nullptr && show_opened_state_act_->isChecked();
  s.show_filter = show_filter_act_ != nullptr && show_filter_act_->isChecked();
  s.show_sidebar = show_sidebar_act_ != nullptr && show_sidebar_act_->isChecked();
  s.read_only = read_only_;
  // Only record a usable width while the sidebar is visible; a hidden splitter
  // child may report 0 and would otherwise clobber the saved width.
  if (main_splitter_ != nullptr && s.show_sidebar) {
    const QList<int> sizes = main_splitter_->sizes();
    if (!sizes.isEmpty() && sizes[0] > 40) {
      s.sidebar_width = sizes[0];
    }
  }
  s.filter_pinned = filter_pinned_;
  s.directories_first = collection_.sorter().directories_first();

  s.sort_key = sort_key_to_settings_string(sort_key_);
  s.sort_ascending = sort_ascending_;
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
  // Always persist the full URL so archive locations keep //archive[:entry].
  s.last_location = QString::fromStdString(location_.as_url());
  s.location_history.clear();
  for (const auto& loc : nav_history_.unique_locations()) {
    s.location_history.append(QString::fromStdString(loc.as_url()));
  }
  save_settings(s);
}

void MainWindow::on_preferences()
{
  AppSettings s = load_settings();
  // Reflect live UI into the struct before editing.
  if (view_mode_ == ViewMode::Icons) {
    s.view_mode = QStringLiteral("icons");
  } else if (view_mode_ == ViewMode::RelativeIcons) {
    s.view_mode = QStringLiteral("relative");
  } else if (view_mode_ == ViewMode::List) {
    s.view_mode = QStringLiteral("small");
  } else {
    s.view_mode = QStringLiteral("detail");
  }
  s.zoom_icons = zoom_.icons;
  s.zoom_list = zoom_.list;
  s.zoom_detail = zoom_.detail;
  s.zoom_index = zoom_.icons;
  s.icon_spacing = icon_spacing_;
  s.icon_cell_padding = icon_cell_padding_;
  s.detail_columns = detail_columns_;
  if (model_ != nullptr) {
    s.icon_detail_level = model_->icon_detail_level();
    s.crop_thumbnails = model_->crop_thumbnails();
  }
  s.show_hidden = collection_.show_hidden();
  s.show_opened_state =
      show_opened_state_act_ != nullptr && show_opened_state_act_->isChecked();
  s.filter_pinned = filter_pinned_;
  // show_filter: if pinned, keep the saved preference; else mirror the action.
  if (filter_pinned_) {
    s.show_filter = load_settings().show_filter;
  } else {
    s.show_filter = show_filter_act_ != nullptr && show_filter_act_->isChecked();
  }
  s.show_sidebar = show_sidebar_act_ != nullptr && show_sidebar_act_->isChecked();
  s.read_only = read_only_;
  s.dismiss_dev_warning = load_settings().dismiss_dev_warning;
  s.directories_first = collection_.sorter().directories_first();

  s.sort_key = sort_key_to_settings_string(sort_key_);
  s.sort_ascending = sort_ascending_;
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
  case collection::GroupMode::Session:
    s.group_mode = QStringLiteral("session");
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


void MainWindow::on_checksums()
{
  QStringList paths;
  for (const auto& fi : selected_fileinfos()) {
    if (fi.is_regular_file() && !fi.is_synthetic()) {
      paths << QString::fromStdString(fi.path().string());
    }
  }
  show_checksum_dialog(this, paths);
}


void MainWindow::on_tag_selected()
{
  tag_.tag_files(selected_fileinfos());
}

void MainWindow::on_tag_manager()
{
  auto* dlg = new TagManagerDialog(this);
  connect(dlg, &TagManagerDialog::tags_changed, this, [this] {
    tag_paint_detail::clear_tag_chip_cache();
    filter_search_.refresh_filter_completions();
    if (graphics_view_ != nullptr) {
      graphics_view_->viewport()->update();
    }
    if (icon_view_ != nullptr) {
      icon_view_->viewport()->update();
    }
    if (tree_view_ != nullptr) {
      tree_view_->viewport()->update();
    }
  });
  connect(dlg, &TagManagerDialog::show_tag_files, this, [this, dlg](const QString& tag) {
    open_tag_collection(tag);
    dlg->close();
  });
  dlg->show();
  dlg->raise();
  dlg->activateWindow();
}

void MainWindow::open_tag_collection(const QString& tag_name)
{
  const QString key = tag_name.trimmed();
  if (key.isEmpty()) {
    return;
  }
  open_location(fs::Location::from_tag(key.toStdString()), true);
}

void MainWindow::load_tag_location_listing()
{
  if (!location_.is_tag()) {
    return;
  }
  const QString key = QString::fromStdString(location_.tag_query());
  if (key.isEmpty()) {
    set_status(QStringLiteral("Empty tag:// location"));
    collection_.clear();
    refresh_list();
    return;
  }

  ActivityMonitor::instance().set_task(QStringLiteral("tag-view"),
                                       QStringLiteral("Loading tag:%1…").arg(key), -1, -1);

  dirtoo::tags::TagStore store;
  std::string err;
  if (!store.open(dirtoo::tags::TagStore::default_path(), &err)) {
    ActivityMonitor::instance().clear_task(QStringLiteral("tag-view"));
    QMessageBox::warning(this, QStringLiteral("Tags"),
                         QStringLiteral("Cannot open tags database:\n%1")
                             .arg(QString::fromStdString(err)));
    return;
  }

  // tag://a,b → union of files matching any listed tag name.
  QStringList parts = key.split(QLatin1Char(','), Qt::SkipEmptyParts);
  std::vector<dirtoo::fs::FileInfo> items;
  std::set<std::string> seen;
  int missing = 0;
  for (QString part : parts) {
    part = part.trimmed();
    if (part.isEmpty()) {
      continue;
    }
    const auto tagged = store.files_for_tag(part.toStdString());
    for (const auto& tf : tagged) {
      for (const auto& path_str : tf.paths) {
        if (!seen.insert(path_str).second) {
          continue;
        }
        if (path_str.find("://") != std::string::npos) {
          try {
            auto loc = dirtoo::fs::Location::from_url(path_str);
            items.push_back(dirtoo::fs::FileInfo::from_location(loc));
          } catch (...) {
            ++missing;
          }
          continue;
        }
        std::error_code ec;
        const std::filesystem::path p{path_str};
        if (!std::filesystem::exists(p, ec) || ec) {
          ++missing;
          continue;
        }
        items.push_back(dirtoo::fs::FileInfo::from_path(p));
      }
    }
  }

  // Not a recursive search session — tag Location itself blocks disk reload/watcher.
  search_session_.active = false;
  search_session_.results.clear();
  search_session_.batch.clear();

  collection_.set_items(std::move(items));
  filter_search_.set_filter_text({});
  refresh_list();
  request_thumbnails_for_visible();

  ActivityMonitor::instance().clear_task(QStringLiteral("tag-view"));
  QString msg =
      QStringLiteral("%1 file(s) with tag:%2").arg(collection_.visible_items().size()).arg(key);
  if (missing > 0) {
    msg += QStringLiteral(" (%1 missing path(s) skipped)").arg(missing);
  }
  set_status(msg);
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
  zoom_.icons = s.zoom_icons;
  zoom_.list = s.zoom_list;
  zoom_.detail = s.zoom_detail;
  zoom_.clamp_all();
  icon_spacing_ = std::clamp(s.icon_spacing, 0, 48);
  icon_cell_padding_ = std::clamp(s.icon_cell_padding, 0, 240);
  if (!s.detail_columns.isEmpty()) {
    detail_columns_ = s.detail_columns;
  }
  apply_detail_column_visibility();
  apply_icon_zoom();
  if (s.view_mode == QLatin1String("icons")) {
    set_view_mode(ViewMode::Icons);
  } else if (s.view_mode == QLatin1String("relative")
             || s.view_mode == QLatin1String("relativeicons")) {
    set_view_mode(ViewMode::RelativeIcons);
  } else if (s.view_mode == QLatin1String("small") || s.view_mode == QLatin1String("smallicons")) {
    set_view_mode(ViewMode::List);
  } else {
    set_view_mode(ViewMode::Detail);
  }
  collection_.set_show_hidden(s.show_hidden);
  if (show_hidden_act_ != nullptr) {
    show_hidden_act_->setChecked(s.show_hidden);
  }
  collection_.sorter().set_directories_first(s.directories_first);
  if (show_opened_state_act_ != nullptr) {
    show_opened_state_act_->setChecked(s.show_opened_state);
  }
  if (model_ != nullptr) {
    model_->set_show_opened_state(s.show_opened_state);
    model_->set_ui_colors(s.colors);
  }
  {
    const collection::SortKey sk = sort_key_from_settings_string(s.sort_key);
    sort_key_ = sk;
    sort_ascending_ = s.sort_ascending;
    collection_.sorter().set_key(sk);
    collection_.sorter().set_ascending(s.sort_ascending);
    update_sort_toolbar_label();
    request_async_sort();
  }
  {
    collection::GroupMode gm = collection::GroupMode::None;
    const QString g = s.group_mode.toLower();
    if (g == QLatin1String("day")) {
      gm = collection::GroupMode::Day;
    } else if (g == QLatin1String("directory")) {
      gm = collection::GroupMode::Directory;
    } else if (g == QLatin1String("duration")) {
      gm = collection::GroupMode::Duration;
    } else if (g == QLatin1String("session")) {
      gm = collection::GroupMode::Session;
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
  if (filter_search_.filter_row() != nullptr) {
    filter_search_.filter_row()->setVisible(s.show_filter || s.filter_pinned);
  }
  if (show_sidebar_act_ != nullptr) {
    show_sidebar_act_->setChecked(s.show_sidebar);
  }
  if (sidebar_.widget() != nullptr) {
    sidebar_.set_visible(s.show_sidebar);
  }
  if (main_splitter_ != nullptr && s.sidebar_width > 40) {
    QList<int> sizes = main_splitter_->sizes();
    if (sizes.size() >= 2) {
      sizes[0] = s.sidebar_width;
      main_splitter_->setSizes(sizes);
    }
  }
  if (sidebar_.places().model() != nullptr) {
    sidebar_.set_show_hidden(s.show_hidden);
  }
  if (filter_search_.filter_edit() != nullptr) {
    filter_search_.filter_edit()->setVisible(true);
    filter_search_.filter_edit()->setEnabled(true);
  }
  read_only_ = s.read_only;
  if (read_only_act_ != nullptr) {
    read_only_act_->setChecked(s.read_only);
  }
  request_async_sort();
  refresh_list();
}




void MainWindow::create_set_from_selection()
{
  const QString id = file_sets_.create_set_from_selection(selected_fileinfos());
  if (!id.isEmpty() && QApplication::keyboardModifiers() & Qt::AltModifier) {
    open_set_location(id);
  }
}

void MainWindow::add_selection_to_last_set()
{
  file_sets_.add_selection_to_last_set(selected_fileinfos());
  if (location_.is_set()) {
    load_set_location_listing();
  }
}

void MainWindow::open_set_location(const QString& set_id)
{
  const QString key = set_id.trimmed();
  if (key.isEmpty()) {
    return;
  }
  open_location(fs::Location::from_set(key.toStdString()), true);
}

void MainWindow::load_set_location_listing()
{
  if (!location_.is_set()) {
    return;
  }
  const QString key = QString::fromStdString(location_.set_query());
  if (key.isEmpty()) {
    set_status(QStringLiteral("Empty set:// location"));
    collection_.clear();
    refresh_list();
    return;
  }

  ActivityMonitor::instance().set_task(QStringLiteral("set-view"),
                                       QStringLiteral("Loading set…"), -1, -1);

  std::string err;
  auto resolved = file_sets_.resolve_query(key.toStdString(), &err);
  if (!resolved) {
    ActivityMonitor::instance().clear_task(QStringLiteral("set-view"));
    set_status(QStringLiteral("Set not found: %1").arg(QString::fromStdString(err)));
    collection_.clear();
    refresh_list();
    return;
  }

  file_sets_.set_last_set_id(QString::fromStdString(resolved->id));
  const auto members = file_sets_.store().members(resolved->id);
  std::vector<dirtoo::fs::FileInfo> items;
  items.reserve(members.size());
  int missing = 0;
  for (const auto& m : members) {
    const std::string& path_str = m.path_key;
    if (path_str.find("://") != std::string::npos) {
      try {
        auto loc = dirtoo::fs::Location::from_url(path_str);
        items.push_back(dirtoo::fs::FileInfo::from_location(loc));
      } catch (...) {
        ++missing;
      }
      continue;
    }
    std::error_code ec;
    const std::filesystem::path p{path_str};
    if (!std::filesystem::exists(p, ec) || ec) {
      ++missing;
      continue;
    }
    items.push_back(dirtoo::fs::FileInfo::from_path(p));
  }

  search_session_.active = false;
  search_session_.results.clear();
  search_session_.batch.clear();

  const int shown = static_cast<int>(items.size());
  collection_.set_items(std::move(items));
  filter_search_.set_filter_text({});
  refresh_list();
  request_thumbnails_for_visible();

  ActivityMonitor::instance().clear_task(QStringLiteral("set-view"));
  const QString label = resolved->label.empty()
                            ? QString::fromStdString(resolved->id.substr(0, 8))
                            : QString::fromStdString(resolved->label);
  set_status(QStringLiteral("Set “%1”: %2 file%3%4")
                 .arg(label)
                 .arg(shown)
                 .arg(shown == 1 ? QString() : QStringLiteral("s"))
                 .arg(missing > 0 ? QStringLiteral(" (%1 missing)").arg(missing) : QString()),
             5000);
}


void MainWindow::select_set_members_of_focus()
{
  const auto sel = selected_fileinfos();
  if (sel.empty()) {
    set_status(QStringLiteral("Select a file that belongs to a set"), 3000);
    return;
  }
  const auto& fi = sel.front();
  std::string key;
  if (fi.location().is_archive()) {
    key = fi.location().as_url();
  } else {
    std::error_code ec;
    const auto abs = std::filesystem::absolute(fi.path(), ec);
    key = ec ? fi.path().string() : abs.lexically_normal().string();
  }
  const QStringList members = file_sets_.member_paths_for_path(QString::fromStdString(key));
  if (members.isEmpty()) {
    set_status(QStringLiteral("File is not in any set"), 3000);
    return;
  }
  // Select rows in the current model whose path matches a member.
  if (model_ == nullptr) {
    return;
  }
  QItemSelection selection;
  int matched = 0;
  for (int row = 0; row < model_->rowCount(); ++row) {
    const auto* row_fi = model_->file_at(row);
    if (row_fi == nullptr) {
      continue;
    }
    std::string row_key;
    if (row_fi->location().is_archive()) {
      row_key = row_fi->location().as_url();
    } else {
      std::error_code ec;
      const auto abs = std::filesystem::absolute(row_fi->path(), ec);
      row_key = ec ? row_fi->path().string() : abs.lexically_normal().string();
    }
    if (members.contains(QString::fromStdString(row_key))) {
      const QModelIndex idx = model_->index(row, 0);
      selection.select(idx, idx);
      ++matched;
    }
  }
  if (auto* view = current_view()) {
    view->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
  }
  set_status(QStringLiteral("Selected %1 set member%2 in view")
                 .arg(matched)
                 .arg(matched == 1 ? QString() : QStringLiteral("s")),
             4000);
}

} // namespace dirtoo::app

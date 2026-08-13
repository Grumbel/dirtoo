// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"
#include "tag_manager_dialog.hpp"
#include "tag_paint.hpp"
#include "view_zoom.hpp"

#include "app_settings.hpp"
#include "preferences_dialog.hpp"
#include "checksum_dialog.hpp"

#include "dirtoo/fs/location.hpp"
#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/tags/tag_store.hpp"
#include "activity_monitor.hpp"

#include <QInputDialog>
#include <QProgressDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStatusBar>
#include <QLineEdit>
#include "size_format.hpp"
#include <QHeaderView>
#include <filesystem>
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
  filter_search_.set_filter_visible(s.show_filter || s.filter_pinned);
  if (auto* edit = filter_search_.filter_edit()) {
    edit->setVisible(true);
    edit->setEnabled(true);
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
  } else if (view_mode_ == ViewMode::List) {
    s.view_mode = QStringLiteral("small");
  } else {
    s.view_mode = QStringLiteral("detail");
  }
  s.zoom_icons = zoom_.icons;
  s.zoom_list = zoom_.list;
  s.zoom_detail = zoom_.detail;
  s.zoom_index = zoom_.icons;
  s.detail_columns = detail_columns_;
  if (model_ != nullptr) {
    s.icon_detail_level = model_->icon_detail_level();
    s.crop_thumbnails = model_->crop_thumbnails();
  }
  s.show_hidden = collection_.show_hidden();
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
  } else if (view_mode_ == ViewMode::List) {
    s.view_mode = QStringLiteral("small");
  } else {
    s.view_mode = QStringLiteral("detail");
  }
  s.zoom_icons = zoom_.icons;
  s.zoom_list = zoom_.list;
  s.zoom_detail = zoom_.detail;
  s.zoom_index = zoom_.icons;
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

  // Leave any live recursive search; tag view is its own result session.
  stop_search();

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

  const auto tagged = store.files_for_tag(key.toStdString());
  std::vector<dirtoo::fs::FileInfo> items;
  items.reserve(tagged.size());
  std::set<std::string> seen;
  int missing = 0;
  for (const auto& tf : tagged) {
    for (const auto& path_str : tf.paths) {
      if (!seen.insert(path_str).second) {
        continue;
      }
      // Archive member URLs stay as path keys; FileInfo::from_path only works for
      // real files. Skip non-existent paths (moved/deleted) with a count.
      if (path_str.find("://") != std::string::npos) {
        // Virtual / archive URL — try Location-based synthetic entry.
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

  // Search-session style: watcher must not wipe this synthetic listing until
  // the user navigates to a real directory.
  search_session_.active = true;
  search_session_.results.clear();
  search_session_.batch.clear();
  search_session_.status_matched = 0;

  collection_.set_items(std::move(items));
  filter_search_.set_filter_text({});
  refresh_list();
  request_thumbnails_for_visible();

  const QString url = QStringLiteral("tag://%1").arg(key);
  location_chrome_.show_line_edit();
  if (location_chrome_.edit() != nullptr) {
    location_chrome_.edit()->setText(url);
  }

  ActivityMonitor::instance().clear_task(QStringLiteral("tag-view"));
  QString msg = QStringLiteral("%1 file(s) with tag:%2").arg(collection_.visible_items().size()).arg(key);
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
  if (!s.detail_columns.isEmpty()) {
    detail_columns_ = s.detail_columns;
  }
  apply_detail_column_visibility();
  apply_icon_zoom();
  if (s.view_mode == QLatin1String("icons")) {
    set_view_mode(ViewMode::Icons);
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
  request_async_sort();
  refresh_list();
}



} // namespace dirtoo::app

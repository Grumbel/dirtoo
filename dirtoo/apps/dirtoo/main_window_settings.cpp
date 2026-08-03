#include "main_window.hpp"
#include "app_settings.hpp"
#include "preferences_dialog.hpp"
#include "theme_icons.hpp"
#include "size_format.hpp"
#include "directory_tree_model.hpp"

#include <QHeaderView>
#include <QLineEdit>
#include <QSplitter>
#include <QTreeView>
#include <QListView>
#include <QToolButton>
#include <QAction>

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
  // Directory tree sidebar visibility + width (persisted as ui/show_sidebar,
  // ui/sidebar_width). Apply before restoreGeometry so the splitter layout is
  // consistent with the saved window size.
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
  s.last_location = QString::fromStdString(location_.as_path().string());
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



} // namespace dirtoo::app

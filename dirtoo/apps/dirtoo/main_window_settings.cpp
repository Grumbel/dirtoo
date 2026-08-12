// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"
#include "view_zoom.hpp"

#include "app_settings.hpp"
#include "preferences_dialog.hpp"
#include "checksum_dialog.hpp"
#include "tag_job.hpp"
#include "tag_paint.hpp"

#include "dirtoo/fs/location.hpp"

#include <QInputDialog>
#include <QProgressDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStatusBar>
#include <QLineEdit>
#include "size_format.hpp"
#include <QHeaderView>

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
  const auto infos = selected_fileinfos();
  std::vector<fs::FileInfo> files;
  files.reserve(infos.size());
  for (const auto& fi : infos) {
    if (!fi.is_regular_file()) {
      continue;
    }
    if (fi.is_synthetic() && !fi.location().is_archive()) {
      continue;
    }
    files.push_back(fi);
  }
  if (files.empty()) {
    QMessageBox::information(this, QStringLiteral("Tag"),
                             QStringLiteral("Select one or more regular files first "
                                            "(disk files or archive members)."));
    return;
  }

  bool ok = false;
  const QString tag = QInputDialog::getText(
      this, QStringLiteral("Tag"),
      QStringLiteral("Tag name for %1 file(s):").arg(static_cast<int>(files.size())),
      QLineEdit::Normal, QString(), &ok);
  if (!ok || tag.trimmed().isEmpty()) {
    return;
  }

  const int total = static_cast<int>(files.size());
  auto* progress = new QProgressDialog(QStringLiteral("Tagging files…"), QStringLiteral("Cancel"), 0,
                                       total, this);
  progress->setWindowModality(Qt::WindowModal);
  progress->setMinimumDuration(total > 1 ? 0 : 2000);
  progress->setAttribute(Qt::WA_DeleteOnClose);
  progress->setValue(0);

  // Hash + archive extract on a worker thread (AGENTS: no hash on GUI).
  auto* job = new TagJob(std::move(files), tag.trimmed(), this);
  connect(progress, &QProgressDialog::canceled, job, &TagJob::cancel);
  connect(job, &TagJob::progress, this, [progress](int done, int total_n, const QString& name) {
    if (progress == nullptr) {
      return;
    }
    progress->setMaximum(total_n);
    progress->setValue(done);
    if (!name.isEmpty()) {
      progress->setLabelText(QStringLiteral("Tagging %1…").arg(name));
    }
  });
  connect(job, &TagJob::failed, this, [this, progress, job](const QString& message) {
    if (progress != nullptr) {
      progress->close();
    }
    QMessageBox::warning(this, QStringLiteral("Tag"), message);
    job->deleteLater();
  });
  connect(job, &TagJob::finished, this,
          [this, progress, job](int tagged, int skipped, const QStringList& problems) {
            if (progress != nullptr) {
              progress->setValue(progress->maximum());
              progress->close();
            }
            QString msg = QStringLiteral("Tagged %1 file(s).").arg(tagged);
            if (skipped > 0) {
              msg += QStringLiteral(" Skipped %1.").arg(skipped);
            }
            if (!problems.isEmpty() && problems.size() <= 5) {
              msg += QLatin1Char('\n') + problems.join(QLatin1Char('\n'));
            }
            if (statusBar() != nullptr) {
              statusBar()->showMessage(msg, 5000);
            }
            if (tagged > 0) {
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
            }
            if (skipped > 0) {
              QMessageBox::warning(this, QStringLiteral("Tag"), msg);
            }
            job->deleteLater();
          });
  job->start();
  progress->show();
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
  if (filter_row_ != nullptr) {
    filter_row_->setVisible(s.show_filter || s.filter_pinned);
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
  if (filter_edit_ != nullptr) {
    filter_edit_->setVisible(true);
    filter_edit_->setEnabled(true);
  }
  request_async_sort();
  refresh_list();
}



} // namespace dirtoo::app

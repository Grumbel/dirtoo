// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"
#include "view_zoom.hpp"

#include "file_item_delegate.hpp"
#include "app_settings.hpp"
#include <QHeaderView>
#include <QStackedWidget>

namespace dirtoo::app {

void MainWindow::apply_icon_zoom()
{
  if (view_mode_ == ViewMode::List) {
    // Windows 95 Explorer "List" view: small icon left of filename, columns
    // filled top-to-bottom then left-to-right.
    static constexpr int kSmall[] = {16, 24, 32, 48, 64, 96, 128};
    const int zi = std::clamp(zoom_.list, 0, static_cast<int>(std::size(kSmall)) - 1);
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
    const int zi = std::clamp(zoom_.detail, 0, static_cast<int>(std::size(kDetail)) - 1);
    const int size = kDetail[zi];
    if (tree_view_ != nullptr) {
      tree_view_->setIconSize(QSize(size, size));
    }
    return;
  }

  const int size = ViewZoom::kIconLevels[std::clamp(zoom_.icons, 0, static_cast<int>(std::size(ViewZoom::kIconLevels)) - 1)];
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


void MainWindow::on_zoom_in()
{
  const int max_zi = (view_mode_ == ViewMode::Icons)
                         ? static_cast<int>(std::size(ViewZoom::kIconLevels)) - 1
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
  } else if (mode == ViewMode::List) {
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
  set_view_mode(ViewMode::List);
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

void MainWindow::apply_detail_column_visibility()
{
  if (tree_view_ == nullptr || model_ == nullptr) {
    return;
  }
  // Name (0) always visible.
  auto visible = [this](const char* key) {
    if (detail_columns_.isEmpty()) {
      // Defaults: everything except optional Width/Height and extra time columns.
      return QLatin1String(key) != QLatin1String("width")
          && QLatin1String(key) != QLatin1String("height")
          && QLatin1String(key) != QLatin1String("accessed")
          && QLatin1String(key) != QLatin1String("changed")
          && QLatin1String(key) != QLatin1String("birth");
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
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Accessed), !visible("accessed"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Changed), !visible("changed"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Birth), !visible("birth"));
  tree_view_->setColumnHidden(static_cast<int>(FileListColumn::Type), !visible("type"));
}


} // namespace dirtoo::app

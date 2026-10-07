# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Pieces shared with the unit tests, compiled once.
add_library(dirtoo-app-core STATIC
  clipboard.cpp
  clipboard.hpp
  directory_load_worker.cpp
  directory_load_worker.hpp
  desktop_entry.cpp
  desktop_entry.hpp
  atomic_write.cpp
  atomic_write.hpp
  path_completion_worker.cpp
  path_completion_worker.hpp
  thread_util.cpp
  thread_util.hpp
)
target_include_directories(dirtoo-app-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dirtoo-app-core
  PUBLIC dirtoo::fs dirtoo::dirops Qt6::Core Qt6::Gui Qt6::Concurrent
  PRIVATE dirtoo_warnings)

qt_add_executable(dirtoo-app
  main.cpp
  async_io.cpp
  async_io.hpp
  file_icons.cpp
  file_icons.hpp
  conflict_probe.cpp
  conflict_probe.hpp
  path_availability.cpp
  path_availability.hpp
  main_window.cpp
  main_window.hpp
  main_window_setup.cpp
  main_window_setup_toolbar.cpp
  main_window_setup_menus.cpp
  main_window_setup_menus_file.cpp
  main_window_setup_menus_edit.cpp
  main_window_setup_menus_view.cpp
  main_window_setup_menus_sort.cpp
  main_window_setup_menus_go.cpp
  main_window_setup_central.cpp
  main_window_thumbs.cpp
  thumbnail_coordinator.cpp
  mime_util.cpp
  mime_util.hpp
  ui_colors.cpp
  ui_colors.hpp
  main_window_transfer.cpp
  main_window_events.cpp
  main_window_settings.cpp
  main_window_view.cpp
  main_window_filter.cpp
  main_window_nav.cpp
  main_window_load.cpp
  main_window_ops.cpp
  main_window_sort.cpp
  main_window_actions.cpp
  main_window_context.cpp
  main_window_location.cpp
  main_window_sidebar.cpp
  file_list_model.cpp
  file_list_model_thumbs.cpp
  file_list_model.hpp
  file_views.hpp
  drag_action_overlay.cpp
  drag_action_overlay.hpp
  file_item_delegate.cpp
  file_item_delegate.hpp
  graphics_file_view.cpp
  graphics_file_view_selection.cpp
  graphics_file_view_dnd.cpp
  graphics_file_view.hpp
  graphics_file_item.cpp
  graphics_file_item.hpp
  conflict_dialog.cpp
  conflict_dialog.hpp
  transfer_dialog.cpp
  transfer_dialog.hpp
  transfer_worker.cpp
  transfer_worker.hpp
  transfer_controller.cpp
  transfer_controller.hpp
  devices_controller.cpp
  devices_controller.hpp
  navigation_history.cpp
  navigation_history.hpp
  search_controller.cpp
  search_controller.hpp
  properties_dialog.cpp
  properties_dialog.hpp
  archive_listing.cpp
  archive_listing.hpp
  file_context_menu.cpp
  file_context_menu.hpp
  archive_member_cache.cpp
  archive_member_cache.hpp
  app_settings.cpp
  app_settings.hpp
  sort_settings.cpp
  sort_settings.hpp
  path_display.cpp
  path_display.hpp
  mutation_support.cpp
  mutation_support.hpp
  hash_service.cpp
  hash_service.hpp
  size_format.cpp
  size_format.hpp
  open_with.cpp
  open_with.hpp
  open_history.cpp
  open_history.hpp
  opened_files_store.cpp
  opened_files_store.hpp
  operations_history.cpp
  operations_history.hpp
  about_dialog.cpp
  about_dialog.hpp
  name_input_dialog.cpp
  name_input_dialog.hpp
  location_button_bar.cpp
  location_button_bar.hpp
  leap_widget.cpp
  leap_widget.hpp
  preferences_dialog.cpp
  preferences_dialog.hpp
  checksum_dialog.cpp
  checksum_dialog.hpp
  tag_job.cpp
  tag_job.hpp
  history_menu.cpp
  history_menu.hpp
  message_area.cpp
  message_area.hpp
  bookmarks.cpp
  bookmarks.hpp
  search_worker.cpp
  search_worker.hpp
  path_completion_service.cpp
  location_chrome.cpp
  filter_search_chrome.cpp
  filter_expression_completer.cpp
  list_pipeline_workers.cpp
  sidebar_places.cpp
  sidebar_controller.cpp
  filter_history.cpp
  tag_controller.cpp
  file_set_controller.cpp
  tag_manager_dialog.cpp
  quick_filter_bar.cpp
  flow_layout.cpp
  activity_monitor.cpp
  activity_dialog.cpp
  activity_indicator.cpp
  view_zoom.cpp
  fs_tree_scan_worker.cpp
  treemap_view.cpp
  sort_worker.cpp
  sort_worker.hpp
  filter_worker.cpp
  filter_worker.hpp
  directory_thumbnail_worker.cpp
  directory_thumbnail_worker.hpp
  directory_tree_model.cpp
  directory_tree_model.hpp
  udisks_client.cpp
  udisks_client.hpp
  badge_icons.hpp
  icon_tile_paint.hpp
)

set_target_properties(dirtoo-app PROPERTIES OUTPUT_NAME dirtoo)

# Plain filesystem icons (no Qt resource embedding).
# Build tree: resources/icons; install: share/dirtoo/icons.
# Only for in-tree dev builds: a packaged build must not embed the (store)
# source path; it finds the icons via <prefix>/share/dirtoo/icons.
if(DIRTOO_IN_TREE_LIBS)
  target_compile_definitions(dirtoo-app PRIVATE
    "DIRTOO_ICON_DIR=\"${CMAKE_SOURCE_DIR}/resources/icons\""
  )
endif()

find_package(SQLite3 REQUIRED)

target_link_libraries(dirtoo-app
  PRIVATE
    dirtoo_warnings
    dirtoo-app-core
    dirtoo::fs
    dirtoo::tree
    dirtoo::collection
    dirtoo::filter
    dirtoo::watcher
    dirtoo::thumbnail
    dirtoo::archive
    dirtoo::dirops
    dirtoo::hash
    dirtoo::tags
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::DBus
    Qt6::Svg
    SQLite3::SQLite3
)

install(TARGETS dirtoo-app RUNTIME DESTINATION bin)

install(FILES
  ${CMAKE_SOURCE_DIR}/resources/dirtoo.desktop
  DESTINATION share/applications
)

install(FILES
  ${CMAKE_SOURCE_DIR}/resources/icons/dirtoo.png
  DESTINATION share/icons/hicolor/48x48/apps
  RENAME dirtoo.png
)

install(FILES
  ${CMAKE_SOURCE_DIR}/resources/icons/dirtoo.svg
  DESTINATION share/icons/hicolor/scalable/apps
  RENAME dirtoo.svg
)

install(FILES
  ${CMAKE_SOURCE_DIR}/resources/icons/dirtoo.png
  ${CMAKE_SOURCE_DIR}/resources/icons/dirtoo.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-video.png
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-image.png
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-loading.png
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-error.png
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-locked.png
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-readonly.png
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-nowrite.png
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-readonly.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-nowrite.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-new.png
  ${CMAKE_SOURCE_DIR}/resources/icons/dnd-copy.png
  ${CMAKE_SOURCE_DIR}/resources/icons/dnd-move.png
  ${CMAKE_SOURCE_DIR}/resources/icons/dnd-link.png
  ${CMAKE_SOURCE_DIR}/resources/icons/dnd-none.png
  ${CMAKE_SOURCE_DIR}/resources/icons/dnd-ask.png
  ${CMAKE_SOURCE_DIR}/resources/icons/crop-thumbnails.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/icon-detail-less.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/icon-detail-more.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/list-add.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/list-remove.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/bookmark-new.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/bookmark-remove.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/view-detail.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/view-filter.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/view-list-tree.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/group-by.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/unfold-hierarchy.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/view-hidden.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/view-icons.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/view-relative.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/view-sidebar.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/view-small-icons.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/zoom-in.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/zoom-out.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-image.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-video.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-loading.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-error.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-locked.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/badge-new.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/dnd-copy.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/dnd-move.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/dnd-link.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/dnd-none.svg
  ${CMAKE_SOURCE_DIR}/resources/icons/dnd-ask.svg
  DESTINATION share/dirtoo/icons
)

install(FILES
  ${CMAKE_SOURCE_DIR}/resources/dirtoo.metainfo.xml
  DESTINATION share/metainfo
  RENAME dirtoo.metainfo.xml
)

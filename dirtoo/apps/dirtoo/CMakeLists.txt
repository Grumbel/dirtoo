# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(dirtoo-app
  main.cpp
  main_window.cpp
  main_window.hpp
  file_list_model.cpp
  file_list_model.hpp
  file_views.hpp
  drag_action_overlay.cpp
  drag_action_overlay.hpp
  file_item_delegate.cpp
  file_item_delegate.hpp
  graphics_file_view.cpp
  graphics_file_view.hpp
  graphics_file_item.cpp
  graphics_file_item.hpp
  clipboard.cpp
  clipboard.hpp
  conflict_dialog.cpp
  conflict_dialog.hpp
  transfer_dialog.cpp
  transfer_dialog.hpp
  transfer_worker.cpp
  transfer_worker.hpp
  properties_dialog.cpp
  properties_dialog.hpp
  app_settings.cpp
  app_settings.hpp
  open_with.cpp
  open_with.hpp
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
  history_menu.cpp
  history_menu.hpp
  message_area.cpp
  message_area.hpp
  bookmarks.cpp
  bookmarks.hpp
  search_worker.cpp
  search_worker.hpp
  path_completion_worker.cpp
  path_completion_worker.hpp
  directory_load_worker.cpp
  directory_load_worker.hpp
  sort_worker.cpp
  sort_worker.hpp
  filter_worker.cpp
  filter_worker.hpp
  directory_thumbnail_worker.cpp
  directory_thumbnail_worker.hpp
  resources.qrc
)

set_target_properties(dirtoo-app PROPERTIES OUTPUT_NAME dirtoo)

target_link_libraries(dirtoo-app
  PRIVATE
    dirtoo_warnings
    dirtoo::fs
    dirtoo::collection
    dirtoo::filter
    dirtoo::watcher
    dirtoo::thumbnail
    dirtoo::archive
    dirtoo::dirops
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
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
  ${CMAKE_SOURCE_DIR}/resources/dirtoo.metainfo.xml
  DESTINATION share/metainfo
  RENAME dirtoo.metainfo.xml
)

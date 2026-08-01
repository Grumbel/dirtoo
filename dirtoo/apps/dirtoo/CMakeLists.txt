# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(dirtoo-app
  main.cpp
  main_window.cpp
  main_window.hpp
  file_list_model.cpp
  file_list_model.hpp
  clipboard.cpp
  clipboard.hpp
  conflict_dialog.cpp
  conflict_dialog.hpp
)

set_target_properties(dirtoo-app PROPERTIES OUTPUT_NAME dirtoo)

target_link_libraries(dirtoo-app
  PRIVATE
    dirtoo_warnings
    dirtoo::fs
    dirtoo::collection
    dirtoo::watcher
    dirtoo::dirops
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

install(TARGETS dirtoo-app RUNTIME DESTINATION bin)

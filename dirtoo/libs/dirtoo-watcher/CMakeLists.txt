# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(dirtoo-watcher
  include/dirtoo/watcher/directory_watcher.hpp
  src/directory_watcher.cpp
)

add_library(dirtoo::watcher ALIAS dirtoo-watcher)

target_include_directories(dirtoo-watcher
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(dirtoo-watcher
  PUBLIC
    dirtoo_warnings
    dirtoo::fs
    Qt6::Core
)

target_compile_features(dirtoo-watcher PUBLIC cxx_std_23)
set_target_properties(dirtoo-watcher PROPERTIES AUTOMOC ON)

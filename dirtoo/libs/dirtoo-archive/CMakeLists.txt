# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(dirtoo-archive
  src/archive_manager.cpp
)

add_library(dirtoo::archive ALIAS dirtoo-archive)

target_include_directories(dirtoo-archive
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(dirtoo-archive
  PUBLIC
    dirtoo::fs
    Qt6::Core
  PRIVATE
    dirtoo_warnings
)

target_compile_features(dirtoo-archive PUBLIC cxx_std_23)

set_target_properties(dirtoo-archive PROPERTIES
  AUTOMOC ON
)

# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(dirtoo-thumbnail
  include/dirtoo/thumbnail/thumbnailer.hpp
  src/thumbnailer.cpp
)

add_library(dirtoo::thumbnail ALIAS dirtoo-thumbnail)

target_include_directories(dirtoo-thumbnail
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(dirtoo-thumbnail
  PUBLIC
    dirtoo_warnings
    dirtoo::fs
    Qt6::Core
)

target_compile_features(dirtoo-thumbnail PUBLIC cxx_std_23)
set_target_properties(dirtoo-thumbnail PROPERTIES AUTOMOC ON)

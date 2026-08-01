# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(dirops
  src/error.cpp
  src/ops.cpp
)

add_library(dirtoo::dirops ALIAS dirops)

target_include_directories(dirops
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(dirops
  PUBLIC dirtoo_warnings
)

target_compile_features(dirops PUBLIC cxx_std_23)

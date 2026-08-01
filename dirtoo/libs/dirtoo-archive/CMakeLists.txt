# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(dirtoo-archive
  include/dirtoo/archive/archive_manager.hpp
  src/archive_manager.cpp
  src/archive_index.cpp
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

install(TARGETS dirtoo-archive
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp"
)

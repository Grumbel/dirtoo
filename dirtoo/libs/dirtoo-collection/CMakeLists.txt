# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(dirtoo-collection
  src/file_collection.cpp
)

add_library(dirtoo::collection ALIAS dirtoo-collection)

target_include_directories(dirtoo-collection
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(dirtoo-collection
  PUBLIC
    dirtoo_warnings
    dirtoo::fs
)

target_compile_features(dirtoo-collection PUBLIC cxx_std_23)

install(TARGETS dirtoo-collection
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp"
)

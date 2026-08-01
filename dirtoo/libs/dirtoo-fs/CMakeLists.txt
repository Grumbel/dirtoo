# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(dirtoo-fs
  src/file_info.cpp
  src/location.cpp
)

add_library(dirtoo::fs ALIAS dirtoo-fs)

target_include_directories(dirtoo-fs
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(dirtoo-fs
  PUBLIC dirtoo_warnings
)

target_compile_features(dirtoo-fs PUBLIC cxx_std_23)

install(TARGETS dirtoo-fs
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp"
)

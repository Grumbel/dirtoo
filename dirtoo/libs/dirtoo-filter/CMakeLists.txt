# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PROJECT_VERSION_FULL)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION" PROJECT_VERSION_FULL LIMIT_COUNT 1)
  else()
    set(PROJECT_VERSION_FULL "0.0.0")
  endif()
endif()
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" _ver "${PROJECT_VERSION_FULL}")
if(NOT _ver)
  set(_ver "0.0.0")
endif()

project(dirtoo-filter VERSION ${_ver} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(dirtoo-filter
  src/parser.cpp
  src/predicates.cpp
  src/search.cpp
  src/media_probe.cpp
  src/media_meta_cache.cpp
)
add_library(dirtoo-filter::dirtoo-filter ALIAS dirtoo-filter)
add_library(dirtoo::filter ALIAS dirtoo-filter)

target_include_directories(dirtoo-filter PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
find_package(SQLite3 REQUIRED)
target_link_libraries(dirtoo-filter PRIVATE SQLite3::SQLite3)
target_compile_features(dirtoo-filter PUBLIC cxx_std_23)
target_compile_options(dirtoo-filter PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

install(TARGETS dirtoo-filter EXPORT dirtoo-filterTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT dirtoo-filterTargets FILE dirtoo-filterTargets.cmake
  NAMESPACE dirtoo-filter:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-filter)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dirtoo-filterConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-filterConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-filter)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-filterConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-filterConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-filterConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-filter)

option(DIRTOO_FILTER_BUILD_TOOLS "Build dt-filter CLI" ON)
if(DIRTOO_FILTER_BUILD_TOOLS)
  add_executable(dt-filter tools/dt_filter.cpp)
  target_link_libraries(dt-filter PRIVATE dirtoo-filter)
  install(TARGETS dt-filter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

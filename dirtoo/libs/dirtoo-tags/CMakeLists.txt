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

project(dirtoo-tags VERSION ${_ver} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(SQLite3 REQUIRED)
find_package(dirtoo-hash REQUIRED CONFIG)

add_library(dirtoo-tags
  include/dirtoo/tags/tag_def.hpp
  include/dirtoo/tags/tag_store.hpp
  src/tag_name.cpp
  src/tag_store.cpp
)
add_library(dirtoo::tags ALIAS dirtoo-tags)
add_library(dirtoo-tags::dirtoo-tags ALIAS dirtoo-tags)

target_include_directories(dirtoo-tags PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(dirtoo-tags PUBLIC dirtoo-hash::dirtoo-hash SQLite3::SQLite3)
target_compile_features(dirtoo-tags PUBLIC cxx_std_23)
target_compile_options(dirtoo-tags PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

install(TARGETS dirtoo-tags EXPORT dirtoo-tagsTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT dirtoo-tagsTargets FILE dirtoo-tagsTargets.cmake
  NAMESPACE dirtoo-tags:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-tags)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dirtoo-tagsConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-tagsConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-tags)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-tagsConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-tagsConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-tagsConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-tags)

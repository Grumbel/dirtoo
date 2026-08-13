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

project(dirtoo-watcher VERSION ${_ver} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Qt6 REQUIRED COMPONENTS Core Concurrent)
find_package(dirtoo-fs REQUIRED CONFIG)
qt_standard_project_setup()

add_library(dirtoo-watcher
  include/dirtoo/watcher/directory_watcher.hpp
  src/directory_watcher.cpp
)
add_library(dirtoo::watcher ALIAS dirtoo-watcher)
add_library(dirtoo-watcher::dirtoo-watcher ALIAS dirtoo-watcher)

target_include_directories(dirtoo-watcher PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(dirtoo-watcher PUBLIC dirtoo-fs::dirtoo-fs Qt6::Core Qt6::Concurrent)
target_compile_features(dirtoo-watcher PUBLIC cxx_std_23)
target_compile_options(dirtoo-watcher PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)
set_target_properties(dirtoo-watcher PROPERTIES AUTOMOC ON)

install(TARGETS dirtoo-watcher EXPORT dirtoo-watcherTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT dirtoo-watcherTargets FILE dirtoo-watcherTargets.cmake
  NAMESPACE dirtoo-watcher:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-watcher)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dirtoo-watcherConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-watcherConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-watcher)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-watcherConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-watcherConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-watcherConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-watcher)

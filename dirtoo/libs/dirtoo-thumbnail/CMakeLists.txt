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

project(dirtoo-thumbnail VERSION ${_ver} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Qt6 REQUIRED COMPONENTS Core DBus)
find_package(dirtoo-fs REQUIRED CONFIG)
qt_standard_project_setup()

add_library(dirtoo-thumbnail
  include/dirtoo/thumbnail/thumbnailer.hpp
  src/thumbnailer.cpp
)
add_library(dirtoo::thumbnail ALIAS dirtoo-thumbnail)
add_library(dirtoo-thumbnail::dirtoo-thumbnail ALIAS dirtoo-thumbnail)

target_include_directories(dirtoo-thumbnail PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(dirtoo-thumbnail PUBLIC dirtoo-fs::dirtoo-fs Qt6::Core Qt6::DBus)
target_compile_features(dirtoo-thumbnail PUBLIC cxx_std_23)
target_compile_options(dirtoo-thumbnail PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)
set_target_properties(dirtoo-thumbnail PROPERTIES AUTOMOC ON)

install(TARGETS dirtoo-thumbnail EXPORT dirtoo-thumbnailTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT dirtoo-thumbnailTargets FILE dirtoo-thumbnailTargets.cmake
  NAMESPACE dirtoo-thumbnail:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-thumbnail)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dirtoo-thumbnailConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-thumbnailConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-thumbnail)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-thumbnailConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-thumbnailConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-thumbnailConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-thumbnail)

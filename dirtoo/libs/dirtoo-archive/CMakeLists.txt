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

project(dirtoo-archive VERSION ${_ver} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(dirtoo-fs REQUIRED CONFIG)
qt_standard_project_setup()

option(DIRTOO_ARCHIVE_USE_LIBARCHIVE "Use libarchive for TOC/extract when available" ON)

add_library(dirtoo-archive
  include/dirtoo/archive/archive_manager.hpp
  src/archive_manager.cpp
  src/archive_index.cpp
  src/archive_libarchive.cpp
)
add_library(dirtoo::archive ALIAS dirtoo-archive)
add_library(dirtoo-archive::dirtoo-archive ALIAS dirtoo-archive)

target_include_directories(dirtoo-archive PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(dirtoo-archive PUBLIC dirtoo-fs::dirtoo-fs Qt6::Core)

if(DIRTOO_ARCHIVE_USE_LIBARCHIVE)
  find_package(LibArchive)
  if(LibArchive_FOUND)
    target_compile_definitions(dirtoo-archive PRIVATE DIRTOO_HAS_LIBARCHIVE=1)
    target_link_libraries(dirtoo-archive PRIVATE LibArchive::LibArchive)
    message(STATUS "dirtoo-archive: libarchive enabled")
  else()
    message(STATUS "dirtoo-archive: libarchive not found; using CLI tools (bsdtar/tar/unzip)")
  endif()
endif()
target_compile_features(dirtoo-archive PUBLIC cxx_std_23)
target_compile_options(dirtoo-archive PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)
set_target_properties(dirtoo-archive PROPERTIES AUTOMOC ON)

install(TARGETS dirtoo-archive EXPORT dirtoo-archiveTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT dirtoo-archiveTargets FILE dirtoo-archiveTargets.cmake
  NAMESPACE dirtoo-archive:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-archive)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dirtoo-archiveConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-archiveConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-archive)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-archiveConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-archiveConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/dirtoo-archiveConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirtoo-archive)

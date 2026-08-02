# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Top-level project: GUI application, optional tools/tests.
# Libraries are independent CMake packages (libs/*/); consume via find_package.

cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PROJECT_VERSION_FULL)
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PROJECT_VERSION_FULL LIMIT_COUNT 1)
endif()
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" PROJECT_VERSION_CMAKE "${PROJECT_VERSION_FULL}")
if(NOT PROJECT_VERSION_CMAKE)
  set(PROJECT_VERSION_CMAKE "0.0.0")
endif()

project(dirtoo
  VERSION ${PROJECT_VERSION_CMAKE}
  DESCRIPTION "Modular Qt file manager"
  LANGUAGES CXX)

message(STATUS "dirtoo version: ${PROJECT_VERSION_FULL}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(GNUInstallDirs)

# So qDebug/qWarning fill QMessageLogContext::{file,line,function}.
add_compile_definitions(QT_MESSAGELOGCONTEXT)

option(DIRTOO_BUILD_APP "Build the dirtoo GUI" ON)
option(DIRTOO_BUILD_TOOLS "Build dt-* tools (requires dirops package)" OFF)
option(DIRTOO_BUILD_TESTS "Build unit tests" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Concurrent)
qt_standard_project_setup()

find_package(dirops REQUIRED CONFIG)
find_package(dirtoo-fs REQUIRED CONFIG)
find_package(dirtoo-filter REQUIRED CONFIG)
find_package(dirtoo-collection REQUIRED CONFIG)
find_package(dirtoo-watcher REQUIRED CONFIG)
find_package(dirtoo-thumbnail REQUIRED CONFIG)
find_package(dirtoo-archive REQUIRED CONFIG)

add_compile_definitions(DIRTOO_VERSION="${PROJECT_VERSION_FULL}")

# Compatibility aliases used by existing target_link_libraries in the app.
if(NOT TARGET dirtoo::dirops)
  add_library(dirtoo::dirops ALIAS dirops::dirops)
endif()
if(NOT TARGET dirtoo::filter)
  add_library(dirtoo::filter ALIAS dirtoo-filter::dirtoo-filter)
endif()
if(NOT TARGET dirtoo::fs)
  add_library(dirtoo::fs ALIAS dirtoo-fs::dirtoo-fs)
endif()
if(NOT TARGET dirtoo::collection)
  add_library(dirtoo::collection ALIAS dirtoo-collection::dirtoo-collection)
endif()
if(NOT TARGET dirtoo::watcher)
  add_library(dirtoo::watcher ALIAS dirtoo-watcher::dirtoo-watcher)
endif()
if(NOT TARGET dirtoo::thumbnail)
  add_library(dirtoo::thumbnail ALIAS dirtoo-thumbnail::dirtoo-thumbnail)
endif()
if(NOT TARGET dirtoo::archive)
  add_library(dirtoo::archive ALIAS dirtoo-archive::dirtoo-archive)
endif()

add_library(dirtoo_warnings INTERFACE)
target_compile_options(dirtoo_warnings INTERFACE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)
# Populate QMessageLogContext file/line/function for qDebug/qWarning/…
target_compile_definitions(dirtoo_warnings INTERFACE QT_MESSAGELOGCONTEXT)

if(DIRTOO_BUILD_APP)
  add_subdirectory(apps/dirtoo)
endif()

if(DIRTOO_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(DIRTOO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

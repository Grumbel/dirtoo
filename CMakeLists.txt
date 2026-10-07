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
option(DIRTOO_BUILD_TOOLS "Build dt-* tools" ON)
option(DIRTOO_BUILD_TESTS "Build unit tests" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Concurrent Svg)
qt_standard_project_setup()

# ---------------------------------------------------------------------------
# Libraries: either build from this checkout (monorepo / nix develop) or
# consume prebuilt packages (flake package src has no libs/).
# Mixing in-tree headers with a stale find_package archive causes ABI breaks
# (e.g. FileInfo stack-canary crashes). One or the other — never both.
# ---------------------------------------------------------------------------
set(_dirtoo_default_in_tree OFF)
if(EXISTS "${CMAKE_SOURCE_DIR}/libs/dirtoo-fs/CMakeLists.txt")
  set(_dirtoo_default_in_tree ON)
endif()
option(DIRTOO_IN_TREE_LIBS
  "Build libs/ from this source tree instead of find_package"
  ${_dirtoo_default_in_tree})

if(DIRTOO_IN_TREE_LIBS)
  message(STATUS "dirtoo: DIRTOO_IN_TREE_LIBS=ON — building libraries from libs/")
  # Dependency order (targets must exist before dependents link them).
  # Disable dirops CLI tools here; top-level DIRTOO_BUILD_TOOLS owns tools/.
  set(DIROPS_BUILD_TOOLS OFF CACHE BOOL "Build dt-* CLI tools" FORCE)
  add_subdirectory(libs/dirtoo-fs)
  add_subdirectory(libs/dirtoo-tree)
  add_subdirectory(libs/dirops)
  add_subdirectory(libs/dirtoo-hash)
  add_subdirectory(libs/dirtoo-tags)
  add_subdirectory(libs/dirtoo-filter)
  add_subdirectory(libs/dirtoo-collection)
  add_subdirectory(libs/dirtoo-watcher)
  add_subdirectory(libs/dirtoo-thumbnail)
  add_subdirectory(libs/dirtoo-archive)
else()
  message(STATUS "dirtoo: DIRTOO_IN_TREE_LIBS=OFF — find_package for libraries")
  find_package(dirops REQUIRED CONFIG)
  find_package(dirtoo-fs REQUIRED CONFIG)
  find_package(dirtoo-tree REQUIRED CONFIG)
  find_package(dirtoo-filter REQUIRED CONFIG)
  find_package(dirtoo-collection REQUIRED CONFIG)
  find_package(dirtoo-watcher REQUIRED CONFIG)
  find_package(dirtoo-thumbnail REQUIRED CONFIG)
  find_package(dirtoo-archive REQUIRED CONFIG)
  find_package(dirtoo-hash REQUIRED CONFIG)
  find_package(dirtoo-tags REQUIRED CONFIG)
endif()

add_compile_definitions(DIRTOO_VERSION="${PROJECT_VERSION_FULL}")

# Compatibility aliases for the app (dirtoo::* names).
# CMake forbids ALIAS-of-ALIAS: in-tree the real target (dirops, dirtoo-fs, …)
# is aliased; with find_package the imported <pkg>::<pkg> target is.
foreach(_lib IN ITEMS dirops fs tree filter collection watcher thumbnail archive hash tags)
  if(TARGET dirtoo::${_lib})
    continue()
  endif()
  if(_lib STREQUAL "dirops")
    set(_pkg dirops)
  else()
    set(_pkg dirtoo-${_lib})
  endif()
  if(TARGET ${_pkg})
    add_library(dirtoo::${_lib} ALIAS ${_pkg})
  elseif(TARGET ${_pkg}::${_pkg})
    add_library(dirtoo::${_lib} ALIAS ${_pkg}::${_pkg})
  endif()
endforeach()

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

# Manual pages (GUI + tools); install does not require tools to be built.
add_subdirectory(man)

if(DIRTOO_BUILD_TESTS)
  if(NOT DIRTOO_BUILD_APP)
    message(FATAL_ERROR "DIRTOO_BUILD_TESTS requires DIRTOO_BUILD_APP (tests link dirtoo-app-core)")
  endif()
  enable_testing()
  add_subdirectory(tests)
endif()

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
# CMake forbids ALIAS-of-ALIAS: in-tree libs already define dirtoo::* (and
# dirops::dirops) as aliases of the real target — only alias the real target,
# or the imported package target when not building in-tree.
function(dirtoo_alias_compat pretty real_or_pkg)
  if(TARGET "${pretty}")
    return()
  endif()
  if(TARGET "${real_or_pkg}")
    add_library("${pretty}" ALIAS "${real_or_pkg}")
  endif()
endfunction()

# Prefer the real library target when present (in-tree); else the namespaced
# imported target from find_package.
dirtoo_alias_compat(dirtoo::dirops dirops)
if(NOT TARGET dirtoo::dirops)
  dirtoo_alias_compat(dirtoo::dirops dirops::dirops)
endif()
dirtoo_alias_compat(dirtoo::fs dirtoo-fs)
if(NOT TARGET dirtoo::fs)
  dirtoo_alias_compat(dirtoo::fs dirtoo-fs::dirtoo-fs)
endif()
dirtoo_alias_compat(dirtoo::filter dirtoo-filter)
if(NOT TARGET dirtoo::filter)
  dirtoo_alias_compat(dirtoo::filter dirtoo-filter::dirtoo-filter)
endif()
dirtoo_alias_compat(dirtoo::collection dirtoo-collection)
if(NOT TARGET dirtoo::collection)
  dirtoo_alias_compat(dirtoo::collection dirtoo-collection::dirtoo-collection)
endif()
dirtoo_alias_compat(dirtoo::watcher dirtoo-watcher)
if(NOT TARGET dirtoo::watcher)
  dirtoo_alias_compat(dirtoo::watcher dirtoo-watcher::dirtoo-watcher)
endif()
dirtoo_alias_compat(dirtoo::thumbnail dirtoo-thumbnail)
if(NOT TARGET dirtoo::thumbnail)
  dirtoo_alias_compat(dirtoo::thumbnail dirtoo-thumbnail::dirtoo-thumbnail)
endif()
dirtoo_alias_compat(dirtoo::archive dirtoo-archive)
if(NOT TARGET dirtoo::archive)
  dirtoo_alias_compat(dirtoo::archive dirtoo-archive::dirtoo-archive)
endif()
dirtoo_alias_compat(dirtoo::hash dirtoo-hash)
if(NOT TARGET dirtoo::hash)
  dirtoo_alias_compat(dirtoo::hash dirtoo-hash::dirtoo-hash)
endif()
dirtoo_alias_compat(dirtoo::tags dirtoo-tags)
if(NOT TARGET dirtoo::tags)
  dirtoo_alias_compat(dirtoo::tags dirtoo-tags::dirtoo-tags)
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

# Manual pages (GUI + tools); install does not require tools to be built.
add_subdirectory(man)

if(DIRTOO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

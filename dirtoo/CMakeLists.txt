# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)

# Source of truth: top-level VERSION file (e.g. "0.2.0-dev"), or
# -DPROJECT_VERSION_FULL=... from packaging (Nix flake appends +g<rev>).
if(NOT DEFINED PROJECT_VERSION_FULL)
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PROJECT_VERSION_FULL LIMIT_COUNT 1)
endif()

# CMake project() VERSION expects major.minor.patch (no -dev / +g suffix).
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" PROJECT_VERSION_CMAKE "${PROJECT_VERSION_FULL}")
if(NOT PROJECT_VERSION_CMAKE)
  set(PROJECT_VERSION_CMAKE "0.0.0")
endif()

project(dirtoo
  VERSION ${PROJECT_VERSION_CMAKE}
  DESCRIPTION "Modular Qt file manager and filesystem tools"
  LANGUAGES CXX)

message(STATUS "dirtoo version: ${PROJECT_VERSION_FULL}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

option(DIRTOO_WITH_QT "Build Qt-based libraries and the GUI" ON)
option(DIRTOO_BUILD_APP "Build the dirtoo GUI application" ON)
option(DIRTOO_BUILD_TOOLS "Build dirops CLI tools" ON)
option(DIRTOO_BUILD_TESTS "Build unit tests" ON)
option(DIRTOO_BUILD_DIROPS "Build dirops library" ON)
option(DIRTOO_BUILD_FS "Build dirtoo-fs library" ON)
option(DIRTOO_BUILD_COLLECTION "Build dirtoo-collection library" ON)
option(DIRTOO_BUILD_WATCHER "Build dirtoo-watcher library" ON)
option(DIRTOO_BUILD_THUMBNAIL "Build dirtoo-thumbnail library" ON)
option(DIRTOO_BUILD_ARCHIVE "Build dirtoo-archive library" ON)

# App/tests imply Qt components.
if(DIRTOO_BUILD_APP OR DIRTOO_BUILD_TESTS)
  set(DIRTOO_WITH_QT ON)
endif()

if(DIRTOO_WITH_QT)
  find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus)
  qt_standard_project_setup()
endif()

add_compile_definitions(DIRTOO_VERSION="${PROJECT_VERSION_FULL}")

add_library(dirtoo_warnings INTERFACE)
target_compile_options(dirtoo_warnings INTERFACE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

# Qt-free core
if(DIRTOO_BUILD_DIROPS)
  add_subdirectory(libs/dirops)
endif()
if(DIRTOO_BUILD_FS)
  add_subdirectory(libs/dirtoo-fs)
endif()
if(DIRTOO_BUILD_COLLECTION)
  if(NOT DIRTOO_BUILD_FS)
    message(FATAL_ERROR "dirtoo-collection requires DIRTOO_BUILD_FS=ON")
  endif()
  add_subdirectory(libs/dirtoo-collection)
endif()

# Qt libraries
if(DIRTOO_WITH_QT)
  if(DIRTOO_BUILD_WATCHER)
    if(NOT DIRTOO_BUILD_FS)
      message(FATAL_ERROR "dirtoo-watcher requires DIRTOO_BUILD_FS=ON")
    endif()
    add_subdirectory(libs/dirtoo-watcher)
  endif()
  if(DIRTOO_BUILD_THUMBNAIL)
    if(NOT DIRTOO_BUILD_FS)
      message(FATAL_ERROR "dirtoo-thumbnail requires DIRTOO_BUILD_FS=ON")
    endif()
    add_subdirectory(libs/dirtoo-thumbnail)
  endif()
  if(DIRTOO_BUILD_ARCHIVE)
    if(NOT DIRTOO_BUILD_FS)
      message(FATAL_ERROR "dirtoo-archive requires DIRTOO_BUILD_FS=ON")
    endif()
    add_subdirectory(libs/dirtoo-archive)
  endif()
endif()

if(DIRTOO_BUILD_APP)
  if(NOT DIRTOO_WITH_QT)
    message(FATAL_ERROR "DIRTOO_BUILD_APP requires DIRTOO_WITH_QT=ON")
  endif()
  add_subdirectory(apps/dirtoo)
endif()

if(DIRTOO_BUILD_TOOLS)
  if(NOT DIRTOO_BUILD_DIROPS)
    message(FATAL_ERROR "DIRTOO_BUILD_TOOLS requires DIRTOO_BUILD_DIROPS=ON")
  endif()
  add_subdirectory(tools)
endif()

if(DIRTOO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

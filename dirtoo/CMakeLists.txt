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

option(DIRTOO_BUILD_TESTS "Build unit tests" ON)
option(DIRTOO_BUILD_TOOLS "Build CLI tools" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus)
qt_standard_project_setup()

# Make the full version string available to all targets as DIRTOO_VERSION.
add_compile_definitions(DIRTOO_VERSION="${PROJECT_VERSION_FULL}")

# ---------------------------------------------------------------------------
# Compiler warnings
# ---------------------------------------------------------------------------
add_library(dirtoo_warnings INTERFACE)
target_compile_options(dirtoo_warnings INTERFACE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

# ---------------------------------------------------------------------------
# Libraries
# ---------------------------------------------------------------------------
add_subdirectory(libs/dirops)
add_subdirectory(libs/dirtoo-fs)
add_subdirectory(libs/dirtoo-collection)
add_subdirectory(libs/dirtoo-watcher)
add_subdirectory(libs/dirtoo-thumbnail)
add_subdirectory(libs/dirtoo-archive)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_subdirectory(apps/dirtoo)

# ---------------------------------------------------------------------------
# Tools & tests
# ---------------------------------------------------------------------------
if(DIRTOO_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(DIRTOO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

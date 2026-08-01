# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(dirtoo
  VERSION 0.1.0
  DESCRIPTION "Modular Qt file manager and filesystem tools"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DIRTOO_BUILD_TESTS "Build unit tests" ON)
option(DIRTOO_BUILD_TOOLS "Build CLI tools" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus)
qt_standard_project_setup()

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

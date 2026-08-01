# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Catch2 must be provided by the environment (e.g. nixpkgs catch2_3).
# Do not use FetchContent / ExternalProject.
find_package(Catch2 3 REQUIRED)

add_executable(dirtoo-tests
  test_location.cpp
  test_dirops.cpp
  test_collection.cpp
  test_clipboard_text.cpp
  test_archive_index.cpp
  ${CMAKE_SOURCE_DIR}/apps/dirtoo/clipboard.cpp
)

target_include_directories(dirtoo-tests PRIVATE
  ${CMAKE_SOURCE_DIR}/apps/dirtoo
)

target_link_libraries(dirtoo-tests
  PRIVATE
    dirtoo::fs
    dirtoo::dirops
    dirtoo::collection
    dirtoo::archive
    Qt6::Core
    Catch2::Catch2WithMain
    dirtoo_warnings
)

include(CTest)
include(Catch)
catch_discover_tests(dirtoo-tests)

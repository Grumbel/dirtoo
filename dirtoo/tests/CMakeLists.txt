# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Catch2 3 REQUIRED)

add_executable(dirtoo-tests
  test_location.cpp
  test_dirops.cpp
  test_collection.cpp
  test_clipboard_text.cpp
  test_archive_index.cpp
  test_filter.cpp
  ${CMAKE_SOURCE_DIR}/apps/dirtoo/clipboard.cpp
)

target_include_directories(dirtoo-tests PRIVATE
  ${CMAKE_SOURCE_DIR}/apps/dirtoo
)

target_link_libraries(dirtoo-tests
  PRIVATE
    dirtoo-fs::dirtoo-fs
    dirops::dirops
    dirtoo-collection::dirtoo-collection
    dirtoo-archive::dirtoo-archive
    dirtoo-filter::dirtoo-filter
    Qt6::Core
    Catch2::Catch2WithMain
    dirtoo_warnings
)

include(CTest)
include(Catch)
catch_discover_tests(dirtoo-tests)

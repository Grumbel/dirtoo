# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Catch2 3 provides <catch2/catch_test_macros.hpp>. Without it the GUI still
# builds: pass -DDIRTOO_BUILD_TESTS=OFF or install catch2_3 (nix develop).
find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
  message(WARNING
    "Catch2 3 not found — skipping dirtoo-tests. "
    "Install catch2_3 (e.g. nix develop) or reconfigure with -DDIRTOO_BUILD_TESTS=OFF.")
  return()
endif()
if(NOT TARGET Catch2::Catch2WithMain)
  message(WARNING
    "Catch2 3 found but Catch2::Catch2WithMain is missing — skipping dirtoo-tests.")
  return()
endif()

add_executable(dirtoo-tests
  test_location.cpp
  test_dirops.cpp
  test_collection.cpp
  test_clipboard_text.cpp
  test_archive_index.cpp
  test_filter.cpp
  test_hash.cpp
  test_tag_store.cpp
  test_file_set_store.cpp
  test_directory_load_worker.cpp
  ${CMAKE_SOURCE_DIR}/apps/dirtoo/clipboard.cpp
  ${CMAKE_SOURCE_DIR}/apps/dirtoo/directory_load_worker.cpp
)

target_include_directories(dirtoo-tests PRIVATE
  ${CMAKE_SOURCE_DIR}/apps/dirtoo
)

# Prefer in-tree lib headers over stale find_package installs (BEFORE = first on path).
set(_dirtoo_in_tree_lib_incs "")
foreach(_inc
    "${CMAKE_SOURCE_DIR}/libs/dirtoo-tags/include"
    "${CMAKE_SOURCE_DIR}/libs/dirtoo-fs/include"
    "${CMAKE_SOURCE_DIR}/libs/dirtoo-filter/include"
    "${CMAKE_SOURCE_DIR}/libs/dirtoo-hash/include")
  if(EXISTS "${_inc}")
    list(APPEND _dirtoo_in_tree_lib_incs "${_inc}")
  endif()
endforeach()
if(_dirtoo_in_tree_lib_incs)
  target_include_directories(dirtoo-tests BEFORE PRIVATE ${_dirtoo_in_tree_lib_incs})
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/libs/dirtoo-tags/src/file_set_store.cpp")
  target_sources(dirtoo-tests PRIVATE
    "${CMAKE_SOURCE_DIR}/libs/dirtoo-tags/src/file_set_store.cpp")
endif()
# Location::from_set / set_query — same stale-package problem as FileSetStore.
if(EXISTS "${CMAKE_SOURCE_DIR}/libs/dirtoo-fs/src/location.cpp")
  target_sources(dirtoo-tests PRIVATE
    "${CMAKE_SOURCE_DIR}/libs/dirtoo-fs/src/location.cpp")
endif()

target_link_libraries(dirtoo-tests
  PRIVATE
    dirtoo-fs::dirtoo-fs
    dirops::dirops
    dirtoo-collection::dirtoo-collection
    dirtoo-archive::dirtoo-archive
    dirtoo-filter::dirtoo-filter
    dirtoo-tags::dirtoo-tags
    dirtoo-hash::dirtoo-hash
    Qt6::Core
    Catch2::Catch2WithMain
    dirtoo_warnings
)

# Some Catch2 installs export a usable link target but leave INTERFACE include
# dirs empty on the consumer side — force them when present so
# <catch2/catch_test_macros.hpp> resolves.
foreach(_c2_tgt Catch2::Catch2WithMain Catch2::Catch2)
  if(TARGET ${_c2_tgt})
    get_target_property(_c2_inc ${_c2_tgt} INTERFACE_INCLUDE_DIRECTORIES)
    if(_c2_inc AND NOT _c2_inc STREQUAL "_c2_inc-NOTFOUND")
      target_include_directories(dirtoo-tests PRIVATE ${_c2_inc})
    endif()
  endif()
endforeach()

include(CTest)
include(Catch)
catch_discover_tests(dirtoo-tests)

# Installed so `nix flake check` can run tests against the built package
# without rebuilding (dirtoo package installs the binary; checks run it).
install(TARGETS dirtoo-tests RUNTIME DESTINATION libexec/dirtoo)

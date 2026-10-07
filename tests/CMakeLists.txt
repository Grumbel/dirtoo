# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Catch2 3 is required when tests are enabled (nix develop provides it);
# pass -DDIRTOO_BUILD_TESTS=OFF to build without.
find_package(Catch2 3 REQUIRED)
find_package(LibArchive REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(dirtoo-tests
  test_location.cpp
  test_file_info.cpp
  test_dirops.cpp
  test_collection.cpp
  test_clipboard_text.cpp
  test_archive_index.cpp
  test_archive_extract.cpp
  test_directory_watcher.cpp
  test_desktop_entry.cpp
  test_atomic_write.cpp
  test_path_completion.cpp
  test_thread_util.cpp
  test_archive_listing.cpp
  test_sort_worker.cpp
  test_transfer_worker.cpp
  test_filter.cpp
  test_hash.cpp
  test_tag_store.cpp
  test_file_set_store.cpp
  test_directory_load_worker.cpp
  test_fs_tree.cpp
)

target_link_libraries(dirtoo-tests
  PRIVATE
    dirtoo-app-core
    dirtoo-fs::dirtoo-fs
    dirtoo-tree::dirtoo-tree
    dirops::dirops
    dirtoo-collection::dirtoo-collection
    dirtoo-archive::dirtoo-archive
    dirtoo-watcher::dirtoo-watcher
    LibArchive::LibArchive
    SQLite3::SQLite3
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

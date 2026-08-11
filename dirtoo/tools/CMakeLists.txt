# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# CLI tools. dirops-backed tools always; others when packages are available.

set(_dirops_tools rename mkdir copy move swap rm mkfile symlink)
foreach(tool ${_dirops_tools})
  add_executable(dt-${tool} dt_${tool}.cpp)
  target_link_libraries(dt-${tool} PRIVATE dirops::dirops dirtoo_warnings)
  target_compile_definitions(dt-${tool} PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
  install(TARGETS dt-${tool} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endforeach()

# Empty-directory removal (Python programs/rmdir.py); no recursive file delete.
add_executable(dt-rmdir dt_rmdir.cpp)
if(TARGET dirtoo_warnings)
  target_link_libraries(dt-rmdir PRIVATE dirtoo_warnings)
endif()
target_compile_definitions(dt-rmdir PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
install(TARGETS dt-rmdir RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})


if(TARGET dirtoo::filter OR TARGET dirtoo-filter::dirtoo-filter)
  if(NOT TARGET dt-filter AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../libs/dirtoo-filter/tools/dt_filter.cpp")
    add_executable(dt-filter ../libs/dirtoo-filter/tools/dt_filter.cpp)
    target_link_libraries(dt-filter PRIVATE dirtoo::filter dirtoo_warnings)
    target_compile_definitions(dt-filter PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
    install(TARGETS dt-filter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

  add_executable(dt-mediainfo dt_mediainfo.cpp)
  target_link_libraries(dt-mediainfo PRIVATE dirtoo::filter dirtoo_warnings)
  target_compile_definitions(dt-mediainfo PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
  install(TARGETS dt-mediainfo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(TARGET dirtoo::archive OR TARGET dirtoo-archive::dirtoo-archive)
  add_executable(dt-archiveinfo dt_archiveinfo.cpp)
  target_link_libraries(dt-archiveinfo PRIVATE dirtoo::archive dirtoo_warnings)
  target_compile_definitions(dt-archiveinfo PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
  install(TARGETS dt-archiveinfo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# --- Path / filename quality (no Qt) ------------------------------------------
add_executable(dt-fsck dt_fsck.cpp)
if(TARGET dirtoo_warnings)
  target_link_libraries(dt-fsck PRIVATE dirtoo_warnings)
endif()
target_compile_definitions(dt-fsck PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
install(TARGETS dt-fsck RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# --- Filter-backed find/search ------------------------------------------------
if(TARGET dirtoo::filter OR TARGET dirtoo-filter::dirtoo-filter)
  add_executable(dt-find dt_find.cpp)
  target_link_libraries(dt-find PRIVATE dirtoo::filter dirtoo_warnings)
  target_compile_definitions(dt-find PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
  install(TARGETS dt-find RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  # Python entry points: dt-find and dt-search (same behaviour).
  add_executable(dt-search dt_find.cpp)
  target_link_libraries(dt-search PRIVATE dirtoo::filter dirtoo_warnings)
  target_compile_definitions(dt-search PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
  install(TARGETS dt-search RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# --- Watcher / MIME / desktop / icon / metadata (need Qt) ---------------------
find_package(Qt6 QUIET COMPONENTS Core Gui)
if(Qt6Core_FOUND)
  if(TARGET dirtoo::watcher OR TARGET dirtoo-watcher::dirtoo-watcher)
    add_executable(dt-watch dt_watch.cpp)
    target_link_libraries(dt-watch PRIVATE dirtoo::watcher Qt6::Core dirtoo_warnings)
    target_compile_definitions(dt-watch PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
    set_target_properties(dt-watch PROPERTIES AUTOMOC ON)
    install(TARGETS dt-watch RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

  add_executable(dt-mime dt_mime.cpp)
  target_link_libraries(dt-mime PRIVATE Qt6::Core dirtoo_warnings)
  target_compile_definitions(dt-mime PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
  install(TARGETS dt-mime RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  add_executable(dt-desktop dt_desktop.cpp)
  target_link_libraries(dt-desktop PRIVATE Qt6::Core dirtoo_warnings)
  target_compile_definitions(dt-desktop PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
  install(TARGETS dt-desktop RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  if(Qt6Gui_FOUND)
    add_executable(dt-icon dt_icon.cpp)
    target_link_libraries(dt-icon PRIVATE Qt6::Core Qt6::Gui dirtoo_warnings)
    target_compile_definitions(dt-icon PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
    install(TARGETS dt-icon RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

  if(TARGET dirtoo::filter OR TARGET dirtoo-filter::dirtoo-filter)
    add_executable(dt-metadata dt_metadata.cpp)
    target_link_libraries(dt-metadata PRIVATE dirtoo::filter Qt6::Core dirtoo_warnings)
    target_compile_definitions(dt-metadata PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
    install(TARGETS dt-metadata RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()
endif()

# --- Checksums (dirtoo-hash) --------------------------------------------------
if(TARGET dirtoo::hash OR TARGET dirtoo-hash::dirtoo-hash)
  add_executable(dt-checksum dt_checksum.cpp)
  target_link_libraries(dt-checksum PRIVATE dirtoo::hash dirtoo_warnings)
  target_compile_definitions(dt-checksum PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
  install(TARGETS dt-checksum RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

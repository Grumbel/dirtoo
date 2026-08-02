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

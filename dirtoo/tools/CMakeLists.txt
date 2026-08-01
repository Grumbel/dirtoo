# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Prefer dirops package; tools are primarily built from the dirops package itself.
foreach(tool rename mkdir copy move swap)
  add_executable(dt-${tool} dt_${tool}.cpp)
  target_link_libraries(dt-${tool} PRIVATE dirops::dirops dirtoo_warnings)
  target_compile_definitions(dt-${tool} PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
  install(TARGETS dt-${tool} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endforeach()

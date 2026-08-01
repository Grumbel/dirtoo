# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(dt-rename dt_rename.cpp)
target_link_libraries(dt-rename PRIVATE dirtoo::dirops dirtoo_warnings)

add_executable(dt-mkdir dt_mkdir.cpp)
target_link_libraries(dt-mkdir PRIVATE dirtoo::dirops dirtoo_warnings)

install(TARGETS dt-rename dt-mkdir RUNTIME DESTINATION bin)

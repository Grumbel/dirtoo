# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(dt-rename dt_rename.cpp)
target_link_libraries(dt-rename PRIVATE dirtoo::dirops dirtoo_warnings)

add_executable(dt-mkdir dt_mkdir.cpp)
target_link_libraries(dt-mkdir PRIVATE dirtoo::dirops dirtoo_warnings)

add_executable(dt-copy dt_copy.cpp)
target_link_libraries(dt-copy PRIVATE dirtoo::dirops dirtoo_warnings)

add_executable(dt-move dt_move.cpp)
target_link_libraries(dt-move PRIVATE dirtoo::dirops dirtoo_warnings)

add_executable(dt-swap dt_swap.cpp)
target_link_libraries(dt-swap PRIVATE dirtoo::dirops dirtoo_warnings)

install(TARGETS dt-rename dt-mkdir dt-copy dt-move dt-swap RUNTIME DESTINATION bin)

# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)
project(dirtoo-text-thumb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC OFF)

find_package(Qt6 REQUIRED COMPONENTS Gui)

add_executable(dirtoo-text-thumb src/main.cpp)
target_link_libraries(dirtoo-text-thumb PRIVATE Qt6::Gui)

install(TARGETS dirtoo-text-thumb RUNTIME DESTINATION bin)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/data/text-layout.thumbnailer.in
  ${CMAKE_CURRENT_BINARY_DIR}/text-layout.thumbnailer
  @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/text-layout.thumbnailer
        DESTINATION share/thumbnailers)

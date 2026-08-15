# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)
project(dirtoo-text-thumb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
find_package(Freetype REQUIRED)
find_package(Fontconfig REQUIRED)

add_executable(dirtoo-text-thumb src/main.cpp)
target_include_directories(dirtoo-text-thumb PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${FREETYPE_INCLUDE_DIRS}
)
target_link_libraries(dirtoo-text-thumb PRIVATE
  ZLIB::ZLIB
  Freetype::Freetype
  Fontconfig::Fontconfig
)

install(TARGETS dirtoo-text-thumb RUNTIME DESTINATION bin)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/data/text-layout.thumbnailer.in
  ${CMAKE_CURRENT_BINARY_DIR}/text-layout.thumbnailer
  @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/text-layout.thumbnailer
        DESTINATION share/thumbnailers)

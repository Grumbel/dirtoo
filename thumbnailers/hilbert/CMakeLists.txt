# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Standalone Hilbert-curve binary thumbnailer (no Qt / no dirtoo libs).
cmake_minimum_required(VERSION 3.20)
project(dirtoo-hilbert-thumb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)

add_executable(dirtoo-hilbert-thumb
  src/main.cpp
)
target_include_directories(dirtoo-hilbert-thumb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(dirtoo-hilbert-thumb PRIVATE ZLIB::ZLIB)

install(TARGETS dirtoo-hilbert-thumb RUNTIME DESTINATION bin)

# XDG thumbnailer entry — TryExec/Exec use the installed binary name.
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/data/hilbert-curve.thumbnailer.in
  ${CMAKE_CURRENT_BINARY_DIR}/hilbert-curve.thumbnailer
  @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hilbert-curve.thumbnailer
        DESTINATION share/thumbnailers)

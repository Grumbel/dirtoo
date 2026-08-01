# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PROJECT_VERSION_FULL)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION" PROJECT_VERSION_FULL LIMIT_COUNT 1)
  else()
    set(PROJECT_VERSION_FULL "0.0.0")
  endif()
endif()
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" _ver "${PROJECT_VERSION_FULL}")
if(NOT _ver)
  set(_ver "0.0.0")
endif()

project(dirops VERSION ${_ver} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(dirops
  src/error.cpp
  src/ops.cpp
  src/util.cpp
)
add_library(dirops::dirops ALIAS dirops)

target_include_directories(dirops
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(dirops PUBLIC cxx_std_23)
target_compile_options(dirops PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)
target_compile_definitions(dirops PUBLIC DIROPS_VERSION="${PROJECT_VERSION_FULL}")

set_target_properties(dirops PROPERTIES
  EXPORT_NAME dirops
  OUTPUT_NAME dirops
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)

install(TARGETS dirops
  EXPORT diropsTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT diropsTargets
  FILE diropsTargets.cmake
  NAMESPACE dirops::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirops
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/diropsConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/diropsConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirops
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/diropsConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/diropsConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/diropsConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dirops
)

option(DIROPS_BUILD_TOOLS "Build dt-* CLI tools" ON)
if(DIROPS_BUILD_TOOLS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/dt_copy.cpp")
  # Tools live at repo tools/; only when building from monorepo layout.
  set(DIROPS_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../tools")
  foreach(tool rename mkdir copy move swap)
    add_executable(dt-${tool} ${DIROPS_TOOLS_DIR}/dt_${tool}.cpp)
    target_link_libraries(dt-${tool} PRIVATE dirops::dirops)
    target_compile_definitions(dt-${tool} PRIVATE DIRTOO_VERSION="${PROJECT_VERSION_FULL}")
    install(TARGETS dt-${tool} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endforeach()
endif()

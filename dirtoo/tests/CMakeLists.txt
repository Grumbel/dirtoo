# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.4
)
FetchContent_MakeAvailable(Catch2)

add_executable(dirtoo-tests
  test_location.cpp
  test_dirops.cpp
)
target_link_libraries(dirtoo-tests
  PRIVATE
    dirtoo::fs
    dirtoo::dirops
    Catch2::Catch2WithMain
    dirtoo_warnings
)

include(CTest)
include(Catch)
catch_discover_tests(dirtoo-tests)

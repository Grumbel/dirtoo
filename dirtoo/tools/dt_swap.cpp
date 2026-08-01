// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/ops.hpp"

#include <iostream>

int main(int argc, char* argv[])
{
  if (argc != 3) {
    std::cerr << "usage: dt-swap <path-a> <path-b>\n";
    return 2;
  }

  auto result = dirops::swap_names(argv[1], argv[2]);
  if (!result) {
    std::cerr << result.error().to_string() << '\n';
    return 1;
  }
  return 0;
}

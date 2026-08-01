// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/ops.hpp"

#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
  if (argc != 3) {
    std::cerr << "usage: dt-rename <from> <to>\n";
    return 2;
  }

  dirops::Options opt;
  opt.verbose = true;
  auto result = dirops::rename_path(argv[1], argv[2], opt);
  if (!result) {
    std::cerr << result.error().to_string() << '\n';
    return 1;
  }
  return 0;
}

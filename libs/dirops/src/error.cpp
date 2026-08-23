// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/error.hpp"

#include <format>

namespace dirops {

std::string Error::to_string() const
{
  if (!message_.empty()) {
    return std::format("{}: {} ({})", path_.string(), message_, ec_.message());
  }
  return std::format("{}: {}", path_.string(), ec_.message());
}

} // namespace dirops

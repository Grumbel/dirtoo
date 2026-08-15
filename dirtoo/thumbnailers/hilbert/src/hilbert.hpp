// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Classic Hilbert curve: map between (x, y) on a 2^n × 2^n grid and distance d.
// Reference: Butz / Wikipedia (rotations in place).

#pragma once

#include <cstdint>

namespace hilbert {

/// Rotate/flip a quadrant (used by xy2d / d2xy).
inline void rot(std::uint32_t n, std::uint32_t& x, std::uint32_t& y, std::uint32_t rx,
                std::uint32_t ry)
{
  if (ry == 0) {
    if (rx == 1) {
      x = n - 1 - x;
      y = n - 1 - y;
    }
    const std::uint32_t t = x;
    x = y;
    y = t;
  }
}

/// Distance d along the curve → grid coordinates (x, y). n is the grid width (power of two).
inline void d2xy(std::uint32_t n, std::uint64_t d, std::uint32_t& x, std::uint32_t& y)
{
  x = y = 0;
  for (std::uint32_t s = 1; s < n; s <<= 1) {
    const std::uint32_t rx = static_cast<std::uint32_t>((d / 2) & 1);
    const std::uint32_t ry = static_cast<std::uint32_t>((d ^ rx) & 1);
    rot(s, x, y, rx, ry);
    x += s * rx;
    y += s * ry;
    d /= 4;
  }
}

/// Next power of two ≥ v (v > 0).
inline std::uint32_t next_pow2(std::uint32_t v)
{
  --v;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return v + 1;
}

} // namespace hilbert

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dirtoo::tree {

/// Axis-aligned rectangle in layout space (origin top-left, y grows downward).
struct TreemapRect {
  double x = 0;
  double y = 0;
  double w = 0;
  double h = 0;

  [[nodiscard]] double area() const noexcept { return w * h; }
  [[nodiscard]] bool empty() const noexcept { return w <= 0 || h <= 0; }
};

/// One input node for the layout (weight typically total_size; 0 is ignored).
struct TreemapInput {
  std::string id;
  double weight = 0;
};

/// Laid-out cell corresponding to one input.
struct TreemapCell {
  std::string id;
  TreemapRect rect;
  double weight = 0;
};

/// Squarified treemap (Bruls, Huizing, van Wijk).
///
/// Items with weight ≤ 0 are skipped. Remaining area is partitioned so each
/// cell's area is proportional to weight. Empty @p bounds yields an empty result.
[[nodiscard]] std::vector<TreemapCell>
layout_squarified(std::vector<TreemapInput> items, TreemapRect bounds);

} // namespace dirtoo::tree

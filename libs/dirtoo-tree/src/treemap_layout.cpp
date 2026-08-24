// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/tree/treemap_layout.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace dirtoo::tree {
namespace {

[[nodiscard]] double worst_aspect(double row_sum, double side, double item_weight)
{
  if (row_sum <= 0 || side <= 0 || item_weight <= 0) {
    return 1e300;
  }
  const double s2 = row_sum * row_sum;
  const double w2 = side * side;
  return std::max(w2 * item_weight / s2, s2 / (w2 * item_weight));
}

[[nodiscard]] double row_worst(const std::vector<double>& row_weights, double side)
{
  if (row_weights.empty() || side <= 0) {
    return 1e300;
  }
  double sum = 0;
  double min_w = row_weights.front();
  double max_w = row_weights.front();
  for (double w : row_weights) {
    sum += w;
    min_w = std::min(min_w, w);
    max_w = std::max(max_w, w);
  }
  return std::max(worst_aspect(sum, side, min_w), worst_aspect(sum, side, max_w));
}

/// Layout @p row into @p remaining along the shorter side; shrink remaining.
void layout_row(std::vector<TreemapCell>& out, const std::vector<TreemapInput>& row,
                TreemapRect& remaining)
{
  double sum = 0;
  for (const auto& it : row) {
    sum += it.weight;
  }
  if (sum <= 0 || remaining.empty()) {
    return;
  }

  const bool horizontal = remaining.h >= remaining.w; // stack along height → vertical strip

  if (horizontal) {
    const double strip_w = sum / remaining.h;
    double y = remaining.y;
    for (const auto& it : row) {
      const double h = (it.weight / sum) * remaining.h;
      out.push_back(TreemapCell{it.id, TreemapRect{remaining.x, y, strip_w, h}, it.weight});
      y += h;
    }
    remaining.x += strip_w;
    remaining.w = std::max(0.0, remaining.w - strip_w);
  } else {
    const double strip_h = sum / remaining.w;
    double x = remaining.x;
    for (const auto& it : row) {
      const double w = (it.weight / sum) * remaining.w;
      out.push_back(TreemapCell{it.id, TreemapRect{x, remaining.y, w, strip_h}, it.weight});
      x += w;
    }
    remaining.y += strip_h;
    remaining.h = std::max(0.0, remaining.h - strip_h);
  }
}

} // namespace

std::vector<TreemapCell>
layout_squarified(std::vector<TreemapInput> items, TreemapRect bounds)
{
  std::vector<TreemapCell> out;
  if (bounds.empty() || items.empty()) {
    return out;
  }

  items.erase(std::remove_if(items.begin(), items.end(),
                             [](const TreemapInput& it) { return it.weight <= 0; }),
              items.end());
  if (items.empty()) {
    return out;
  }

  std::sort(items.begin(), items.end(),
            [](const TreemapInput& a, const TreemapInput& b) { return a.weight > b.weight; });

  const double weight_sum =
      std::accumulate(items.begin(), items.end(), 0.0,
                      [](double s, const TreemapInput& it) { return s + it.weight; });
  if (weight_sum <= 0) {
    return out;
  }
  const double scale = bounds.area() / weight_sum;
  for (auto& it : items) {
    it.weight *= scale;
  }

  TreemapRect remaining = bounds;
  std::vector<TreemapInput> row;
  std::vector<double> row_w;
  std::size_t i = 0;

  while (i < items.size() && !remaining.empty()) {
    const double side = remaining.h >= remaining.w ? remaining.h : remaining.w;
    const double current_worst = row.empty() ? 1e300 : row_worst(row_w, side);

    row.push_back(items[i]);
    row_w.push_back(items[i].weight);
    const double new_worst = row_worst(row_w, side);

    if (row.size() > 1 && new_worst > current_worst) {
      row.pop_back();
      row_w.pop_back();
      layout_row(out, row, remaining);
      row.clear();
      row_w.clear();
      continue; // retry items[i] against the new remaining rect
    }
    ++i;
  }

  if (!row.empty() && !remaining.empty()) {
    layout_row(out, row, remaining);
  }

  return out;
}

} // namespace dirtoo::tree

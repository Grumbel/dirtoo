// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>

#include "view_mode.hpp"

namespace dirtoo::app {

/// Per-view zoom indices (icons / list / detail).
struct ViewZoom {
  static constexpr int kIconLevels[] = {48, 64, 96, 128, 192, 256, 384, 512, 768, 1024};
  static constexpr int kIconLevelCount = static_cast<int>(std::size(kIconLevels));

  int icons = 2;
  int list = 2;
  int detail = 2;

  void clamp_all()
  {
    icons = std::clamp(icons, 0, kIconLevelCount - 1);
    list = std::clamp(list, 0, 6);
    detail = std::clamp(detail, 0, 6);
  }

  [[nodiscard]] int& for_mode(ViewMode mode);
  [[nodiscard]] int for_mode(ViewMode mode) const;
  [[nodiscard]] int icon_pixel_size() const
  {
    const int i = std::clamp(icons, 0, kIconLevelCount - 1);
    return kIconLevels[i];
  }
};

} // namespace dirtoo::app

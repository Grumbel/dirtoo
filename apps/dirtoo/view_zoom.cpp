// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "view_zoom.hpp"

#include "view_mode.hpp"

namespace dirtoo::app {

int& ViewZoom::for_mode(ViewMode mode)
{
  switch (mode) {
  case ViewMode::List:
    return list;
  case ViewMode::Detail:
    return detail;
  case ViewMode::Icons:
  default:
    return icons;
  }
}

int ViewZoom::for_mode(ViewMode mode) const
{
  switch (mode) {
  case ViewMode::List:
    return list;
  case ViewMode::Detail:
    return detail;
  case ViewMode::Icons:
  default:
    return icons;
  }
}

} // namespace dirtoo::app

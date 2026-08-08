// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace dirtoo::app {

enum class ViewMode {
  Detail,
  Icons,
  List, // List view: icon+name rows in columns (Win95 Explorer List)
  RelativeIcons, // Icons view; tile size scales with file size (log)
};

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"

#include <cstdint>
#include <vector>

namespace dirtoo::app {

/// In-progress recursive search accumulation (UI-side batching).
struct SearchSession {
  bool active = false;
  std::vector<fs::FileInfo> results;
  std::vector<fs::FileInfo> batch;
  quint64 status_matched = 0;

  void clear()
  {
    active = false;
    results.clear();
    batch.clear();
    status_matched = 0;
  }

  void clear_results()
  {
    results.clear();
    batch.clear();
    status_matched = 0;
  }
};

} // namespace dirtoo::app

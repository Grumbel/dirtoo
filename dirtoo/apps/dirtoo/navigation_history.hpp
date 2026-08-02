// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <optional>
#include <vector>

namespace dirtoo::app {

/// Back/forward stack + unique location list for the History menu.
/// Widget-free so navigation policy stays out of MainWindow.
class NavigationHistory {
public:
  /// Record a navigation. Trims forward entries when branching from mid-stack.
  void push(const fs::Location& location, bool record);

  [[nodiscard]] bool can_go_back() const noexcept;
  [[nodiscard]] bool can_go_forward() const noexcept;

  /// Move index and return the location to open (nullopt if cannot).
  [[nodiscard]] std::optional<fs::Location> go_back();
  [[nodiscard]] std::optional<fs::Location> go_forward();

  [[nodiscard]] int index() const noexcept { return index_; }
  [[nodiscard]] const std::vector<fs::Location>& stack() const noexcept { return stack_; }

  /// Most-recent-last unique locations for the History menu (capped).
  [[nodiscard]] const std::vector<fs::Location>& unique_locations() const noexcept
  {
    return unique_;
  }
  void set_unique_locations(std::vector<fs::Location> locations);
  void clear();

private:
  void remember_unique(const fs::Location& location);

  std::vector<fs::Location> stack_;
  int index_ = -1;
  std::vector<fs::Location> unique_;
  static constexpr std::size_t kUniqueCap = 40;
};

} // namespace dirtoo::app

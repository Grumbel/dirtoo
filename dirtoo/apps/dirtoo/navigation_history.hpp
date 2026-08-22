// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <optional>
#include <vector>

namespace dirtoo::app {

/// One back/forward slot: location URL plus view scroll to restore on return.
struct NavigationEntry {
  fs::Location location;
  /// Vertical scrollbar value when we last left this entry (0 = top).
  int scroll_y = 0;
};

/// Back/forward stack + unique location list for the History menu.
/// Widget-free so navigation policy stays out of MainWindow.
class NavigationHistory {
public:
  /// Record a navigation. Stores \p leaving_scroll_y on the current entry
  /// before branching/trimming, then pushes \p location (scroll starts at 0).
  void push(const fs::Location& location, bool record, int leaving_scroll_y = 0);

  /// Update scroll snapshot for the current stack entry (call before go_back/
  /// go_forward so the page we leave remembers its offset).
  void update_current_scroll(int scroll_y);

  [[nodiscard]] bool can_go_back() const noexcept;
  [[nodiscard]] bool can_go_forward() const noexcept;

  /// Move index and return the entry to open (nullopt if cannot).
  [[nodiscard]] std::optional<NavigationEntry> go_back();
  [[nodiscard]] std::optional<NavigationEntry> go_forward();

  /// Jump the stack cursor to \p index (0-based) without trimming the stack.
  [[nodiscard]] std::optional<NavigationEntry> go_to_index(int index);

  [[nodiscard]] int index() const noexcept { return index_; }
  [[nodiscard]] const std::vector<NavigationEntry>& stack() const noexcept { return stack_; }

  /// Most-recent-last unique locations for the History menu (capped).
  [[nodiscard]] const std::vector<fs::Location>& unique_locations() const noexcept
  {
    return unique_;
  }
  void set_unique_locations(std::vector<fs::Location> locations);
  void clear();

private:
  void remember_unique(const fs::Location& location);

  std::vector<NavigationEntry> stack_;
  int index_ = -1;
  std::vector<fs::Location> unique_;
  static constexpr std::size_t kUniqueCap = 40;
};

} // namespace dirtoo::app

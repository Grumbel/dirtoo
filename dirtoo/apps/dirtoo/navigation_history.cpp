// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "navigation_history.hpp"

#include <algorithm>

namespace dirtoo::app {

void NavigationHistory::push(const fs::Location& location, bool record)
{
  if (!record) {
    return;
  }
  if (index_ >= 0 && index_ + 1 < static_cast<int>(stack_.size())) {
    stack_.erase(stack_.begin() + index_ + 1, stack_.end());
  }
  if (stack_.empty() || stack_.back().as_url() != location.as_url()) {
    stack_.push_back(location);
    index_ = static_cast<int>(stack_.size()) - 1;
  } else {
    index_ = static_cast<int>(stack_.size()) - 1;
  }
  remember_unique(location);
}

bool NavigationHistory::can_go_back() const noexcept
{
  return index_ > 0;
}

bool NavigationHistory::can_go_forward() const noexcept
{
  return index_ >= 0 && index_ + 1 < static_cast<int>(stack_.size());
}

std::optional<fs::Location> NavigationHistory::go_back()
{
  if (!can_go_back()) {
    return std::nullopt;
  }
  --index_;
  return stack_[static_cast<std::size_t>(index_)];
}

std::optional<fs::Location> NavigationHistory::go_forward()
{
  if (!can_go_forward()) {
    return std::nullopt;
  }
  ++index_;
  return stack_[static_cast<std::size_t>(index_)];
}

std::optional<fs::Location> NavigationHistory::go_to_index(int index)
{
  if (index < 0 || index >= static_cast<int>(stack_.size())) {
    return std::nullopt;
  }
  index_ = index;
  return stack_[static_cast<std::size_t>(index_)];
}

void NavigationHistory::set_unique_locations(std::vector<fs::Location> locations)
{
  unique_ = std::move(locations);
  if (unique_.size() > kUniqueCap) {
    unique_.erase(unique_.begin(), unique_.end() - static_cast<std::ptrdiff_t>(kUniqueCap));
  }
}

void NavigationHistory::clear()
{
  stack_.clear();
  index_ = -1;
  unique_.clear();
}

void NavigationHistory::remember_unique(const fs::Location& location)
{
  unique_.erase(std::remove_if(unique_.begin(), unique_.end(),
                               [&](const fs::Location& loc) {
                                 return loc.as_url() == location.as_url();
                               }),
                unique_.end());
  unique_.push_back(location);
  if (unique_.size() > kUniqueCap) {
    unique_.erase(unique_.begin());
  }
}

} // namespace dirtoo::app

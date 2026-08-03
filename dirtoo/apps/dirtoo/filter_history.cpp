// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filter_history.hpp"

namespace dirtoo::app {

void FilterHistory::push(const QString& text)
{
  if (text.isEmpty()) {
    return;
  }
  if (entries_.isEmpty() || entries_.last() != text) {
    entries_.append(text);
    if (entries_.size() > kMaxEntries) {
      entries_.removeFirst();
    }
  }
  index_ = -1;
}

QString FilterHistory::older()
{
  if (entries_.isEmpty()) {
    return {};
  }
  if (index_ < 0) {
    index_ = entries_.size() - 1;
  } else if (index_ > 0) {
    --index_;
  }
  return entries_.at(index_);
}

QString FilterHistory::newer()
{
  if (entries_.isEmpty() || index_ < 0) {
    return {};
  }
  if (index_ + 1 < entries_.size()) {
    ++index_;
    return entries_.at(index_);
  }
  index_ = -1;
  return {};
}

} // namespace dirtoo::app

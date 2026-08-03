// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

namespace dirtoo::app {

/// Bounded filter expression history with up/down navigation index.
class FilterHistory {
public:
  static constexpr int kMaxEntries = 50;

  void push(const QString& text);
  [[nodiscard]] bool empty() const { return entries_.isEmpty(); }
  [[nodiscard]] int size() const { return entries_.size(); }

  /// Move to older entry; empty if none.
  [[nodiscard]] QString older();
  /// Move to newer entry; empty string and resets index past end when exhausted.
  [[nodiscard]] QString newer();

  void reset_index() { index_ = -1; }
  [[nodiscard]] int index() const { return index_; }

  [[nodiscard]] const QStringList& entries() const { return entries_; }
  void set_entries(QStringList entries)
  {
    entries_ = std::move(entries);
    index_ = -1;
  }

private:
  QStringList entries_;
  int index_ = -1;
};

} // namespace dirtoo::app

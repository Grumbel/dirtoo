// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"
#include "dirtoo/fs/location.hpp"

#include <optional>
#include <string>
#include <vector>

namespace dirtoo::collection {

/// Ordered collection of FileInfo with lookup by Location.
class FileCollection {
public:
  void clear();
  void set_items(std::vector<fs::FileInfo> items);
  void add(fs::FileInfo info);
  bool remove(const fs::Location& location);

  [[nodiscard]] std::size_t size() const noexcept { return items_.size(); }
  [[nodiscard]] bool empty() const noexcept { return items_.empty(); }

  [[nodiscard]] const fs::FileInfo& operator[](std::size_t index) const { return items_.at(index); }
  [[nodiscard]] const std::vector<fs::FileInfo>& items() const noexcept { return items_; }

  [[nodiscard]] std::optional<std::size_t> index_of(const fs::Location& location) const;

  void sort_by_name(bool ascending = true);
  void sort_by_size(bool ascending = true);
  void sort_by_mtime(bool ascending = true);

  /// Case-insensitive substring filter on basename. Empty needle = show all.
  void set_name_filter(std::string needle);
  void clear_filter();

  void set_show_hidden(bool show);
  [[nodiscard]] bool show_hidden() const noexcept { return show_hidden_; }

  [[nodiscard]] const std::vector<fs::FileInfo>& visible_items() const noexcept;

private:
  void rebuild_visible();

  std::vector<fs::FileInfo> items_;
  std::vector<fs::FileInfo> visible_;
  std::string name_filter_;
  bool show_hidden_ = false;
};

} // namespace dirtoo::collection

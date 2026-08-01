// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/collection/sorter.hpp"
#include "dirtoo/filter/match_func.hpp"
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

  /// Apply current sorter settings to the underlying item list.
  void apply_sort();

  [[nodiscard]] Sorter& sorter() noexcept { return sorter_; }
  [[nodiscard]] const Sorter& sorter() const noexcept { return sorter_; }

  void set_sort_key(SortKey key);
  void set_sort_ascending(bool ascending);
  void set_directories_first(bool v);

  // Convenience wrappers (keep older call sites working).
  void sort_by_name(bool ascending = true);
  void sort_by_size(bool ascending = true);
  void sort_by_mtime(bool ascending = true);
  void sort_by_extension(bool ascending = true);

  /// Set filter expression (dirtoo-filter DSL). Empty string clears the filter.
  /// On parse error, falls back to simple substring match on the whole string.
  void set_name_filter(std::string expression);
  void clear_filter();

  /// Direct MatchFunc (for tests / advanced callers).
  void set_match_func(filter::MatchFuncPtr func);

  void set_show_hidden(bool show);
  [[nodiscard]] bool show_hidden() const noexcept { return show_hidden_; }

  [[nodiscard]] const std::vector<fs::FileInfo>& visible_items() const noexcept;
  [[nodiscard]] const std::string& filter_expression() const noexcept { return filter_expression_; }
  [[nodiscard]] bool filter_parse_ok() const noexcept { return filter_parse_ok_; }

private:
  void rebuild_visible();

  std::vector<fs::FileInfo> items_;
  std::vector<fs::FileInfo> visible_;
  std::string filter_expression_;
  filter::MatchFuncPtr match_;
  bool filter_parse_ok_ = true;
  bool show_hidden_ = false;
  Sorter sorter_;
};

} // namespace dirtoo::collection

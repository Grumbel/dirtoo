// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"

#include <string>
#include <vector>

namespace dirtoo::collection {

/// Sort key modes (parity with Python Sorter + sort actions).
enum class SortKey {
  Name,        // natural (numeric) basename, case-insensitive
  Size,
  Extension,
  Modified,
  Type,        // extension then name (detail "Type" column)
  Width,
  Height,
  Resolution,  // width * height
  AspectRatio, // width / height
  Duration,    // media duration_ms
  Framerate,
  Permissions,
  Random,
};

/// Natural/numeric sort key pieces for a string (like Python numeric_sort_key).
/// Alternating string/number segments; used for Name sorting.
struct NaturalPiece {
  bool is_number = false;
  std::string text;
  std::uint64_t number = 0;
};

[[nodiscard]] std::vector<NaturalPiece> numeric_sort_key(std::string_view text);

class Sorter {
public:
  void set_key(SortKey key) { key_ = key; }
  [[nodiscard]] SortKey key() const noexcept { return key_; }

  void set_directories_first(bool v) { directories_first_ = v; }
  [[nodiscard]] bool directories_first() const noexcept { return directories_first_; }

  void set_ascending(bool v) { ascending_ = v; }
  [[nodiscard]] bool ascending() const noexcept { return ascending_; }

  /// Stable-ish sort of `items` in place according to current settings.
  void sort(std::vector<fs::FileInfo>& items) const;

  /// Compare two items (-1, 0, 1) honouring directories_first and key only
  /// (not reverse). Used by tests.
  [[nodiscard]] int compare(const fs::FileInfo& a, const fs::FileInfo& b) const;

private:
  SortKey key_ = SortKey::Name;
  bool directories_first_ = true;
  bool ascending_ = true;
};

} // namespace dirtoo::collection

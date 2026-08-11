// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace dirtoo::tags {

struct TagDef {
  std::int64_t id = 0;
  std::string name;   // normalized key
  std::string label;  // display (defaults to name)
  std::string color;  // optional #rrggbb
  std::string badge;  // optional icon id / resource name
};

/// Normalize tag name: trim, lowercase, allow [a-z0-9_/-], collapse repeats.
/// Returns empty string if invalid.
[[nodiscard]] std::string normalize_tag_name(std::string_view raw);

} // namespace dirtoo::tags

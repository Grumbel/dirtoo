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

/// Normalize tag name: trim, lowercase, allow [a-z0-9_/: -], collapse repeats.
/// ':' is kept as a namespace separator. Returns empty string if invalid.
[[nodiscard]] std::string normalize_tag_name(std::string_view raw);

/// Local part after the last ':' (or the whole name if unnamespaced).
[[nodiscard]] inline std::string local_tag_name(std::string_view name)
{
  const auto pos = name.rfind(':');
  if (pos == std::string_view::npos || pos + 1 >= name.size()) {
    return std::string(name);
  }
  return std::string(name.substr(pos + 1));
}

/// True if `tag` matches filter pattern `query` (exact, or local-name match when
/// query has no namespace; supports trailing '*' glob on the local part).
[[nodiscard]] bool tag_name_matches(std::string_view tag, std::string_view query);

} // namespace dirtoo::tags

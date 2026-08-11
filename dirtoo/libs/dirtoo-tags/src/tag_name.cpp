// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/tags/tag_def.hpp"

#include <cctype>

namespace dirtoo::tags {

std::string normalize_tag_name(std::string_view raw)
{
  std::string out;
  out.reserve(raw.size());
  bool prev_sep = true;
  for (unsigned char ch : raw) {
    if (std::isspace(ch)) {
      continue;
    }
    char c = static_cast<char>(std::tolower(ch));
    const bool sep = (c == '/' || c == '_' || c == '-');
    if (std::isalnum(static_cast<unsigned char>(c))) {
      out.push_back(c);
      prev_sep = false;
    } else if (sep) {
      if (!prev_sep && !out.empty()) {
        out.push_back(c == '-' ? '_' : c);
        prev_sep = true;
      }
    } else {
      return {};
    }
  }
  while (!out.empty() && (out.back() == '/' || out.back() == '_')) {
    out.pop_back();
  }
  if (out.empty() || out.front() == '/' || out.front() == '_') {
    return {};
  }
  return out;
}

} // namespace dirtoo::tags

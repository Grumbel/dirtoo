// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/tags/tag_def.hpp"

#include <cctype>

namespace dirtoo::tags {

std::string normalize_tag_name(std::string_view raw)
{
  std::string out;
  out.reserve(raw.size());
  bool prev_sep = true; // treat start as separator so we reject leading seps
  for (unsigned char ch : raw) {
    if (std::isspace(ch)) {
      continue;
    }
    char c = static_cast<char>(std::tolower(ch));
    // ':' is a namespace separator. Keep '-', '_', '/' as distinct separators
    // so tags like location-paris stay readable (do not collapse '-' to '_').
    const bool sep = (c == '/' || c == '_' || c == '-' || c == ':');
    if (std::isalnum(static_cast<unsigned char>(c))) {
      out.push_back(c);
      prev_sep = false;
    } else if (sep) {
      if (!prev_sep && !out.empty()) {
        out.push_back(c);
        prev_sep = true;
      }
    } else {
      return {};
    }
  }
  while (!out.empty()
         && (out.back() == '/' || out.back() == '_' || out.back() == '-' || out.back() == ':')) {
    out.pop_back();
  }
  if (out.empty() || out.front() == '/' || out.front() == '_' || out.front() == '-'
      || out.front() == ':') {
    return {};
  }
  // Reject empty namespace segments ("game::doom" / "game:").
  for (std::size_t i = 0; i + 1 < out.size(); ++i) {
    if (out[i] == ':' && out[i + 1] == ':') {
      return {};
    }
  }
  return out;
}

bool tag_name_matches(std::string_view tag, std::string_view query)
{
  if (query.empty() || tag.empty()) {
    return false;
  }
  // Exact full-name match first.
  if (tag == query) {
    return true;
  }
  const bool query_has_ns = query.find(':') != std::string_view::npos;
  if (query_has_ns) {
    // Explicit namespace: only exact (or glob on full string below).
  } else {
    // Unnamespaced query matches any tag whose local part equals query.
    if (local_tag_name(tag) == query) {
      return true;
    }
  }
  // Glob: trailing * only for now (location-*).
  if (!query.empty() && query.back() == '*') {
    const std::string_view prefix = query.substr(0, query.size() - 1);
    if (prefix.find('*') != std::string_view::npos) {
      return false; // only single trailing * supported
    }
    if (query_has_ns) {
      return tag.size() >= prefix.size()
             && tag.substr(0, prefix.size()) == prefix;
    }
    // Match local part prefix, or full-name prefix for unnamespaced tags.
    const std::string local = local_tag_name(tag);
    if (local.size() >= prefix.size()
        && std::string_view(local).substr(0, prefix.size()) == prefix) {
      return true;
    }
    if (tag.find(':') == std::string_view::npos
        && tag.size() >= prefix.size()
        && tag.substr(0, prefix.size()) == prefix) {
      return true;
    }
  }
  return false;
}

} // namespace dirtoo::tags

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/sets/file_set_store.hpp"

#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace dirtoo::app {
namespace set_membership {

inline void push_path_forms(std::unordered_set<std::string>& out, const std::filesystem::path& p)
{
  const std::string raw = p.string();
  if (raw.empty()) {
    return;
  }
  out.insert(raw);
  if (raw.find("://") != std::string::npos || raw.find("//archive") != std::string::npos) {
    return;
  }
  std::error_code ec;
  const auto abs = std::filesystem::absolute(p, ec);
  if (!ec) {
    out.insert(abs.string());
    out.insert(abs.lexically_normal().string());
  }
  const auto weak = std::filesystem::weakly_canonical(p, ec);
  if (!ec) {
    out.insert(weak.string());
  }
}

[[nodiscard]] inline std::optional<std::string>
resolve_set_id(dirtoo::sets::FileSetStore& store, std::string_view query)
{
  if (query.empty()) {
    return std::nullopt;
  }
  if (auto by_id = store.get_set(query)) {
    return by_id->id;
  }
  if (query.size() >= 8) {
    std::optional<std::string> pref;
    for (const auto& s : store.list_sets()) {
      if (s.id.size() >= query.size() && s.id.compare(0, query.size(), query) == 0) {
        if (pref) {
          return std::nullopt;
        }
        pref = s.id;
      }
    }
    if (pref) {
      return pref;
    }
  }
  std::optional<std::string> by_label;
  for (const auto& s : store.list_sets()) {
    if (s.label == query) {
      if (by_label) {
        return std::nullopt;
      }
      by_label = s.id;
    }
  }
  return by_label;
}

/// True if path is a member of set_id (robust path comparison).
[[nodiscard]] inline bool path_in_set(dirtoo::sets::FileSetStore& store, std::string_view set_id,
                                      const std::filesystem::path& path)
{
  std::unordered_set<std::string> keys;
  push_path_forms(keys, path);
  for (const auto& k : keys) {
    if (store.contains(set_id, k)) {
      return true;
    }
  }
  const auto members = store.members(set_id);
  for (const auto& m : members) {
    for (const auto& k : keys) {
      if (k == m.path_key) {
        return true;
      }
    }
  }
  for (const auto& m : members) {
    std::error_code ec;
    if (std::filesystem::equivalent(path, m.path_key, ec) && !ec) {
      return true;
    }
  }
  const auto item_parent = path.parent_path();
  const auto item_name = path.filename().string();
  if (item_name.empty()) {
    return false;
  }
  for (const auto& m : members) {
    const std::filesystem::path mp{m.path_key};
    if (mp.filename().string() != item_name) {
      continue;
    }
    std::error_code ec;
    if (std::filesystem::equivalent(item_parent, mp.parent_path(), ec) && !ec) {
      return true;
    }
    std::unordered_set<std::string> a;
    std::unordered_set<std::string> b;
    push_path_forms(a, item_parent);
    push_path_forms(b, mp.parent_path());
    for (const auto& x : a) {
      if (b.contains(x)) {
        return true;
      }
    }
  }
  return false;
}

/// Sets that contain this path (tries multiple key forms).
[[nodiscard]] inline std::vector<dirtoo::sets::FileSet>
sets_for_path_robust(dirtoo::sets::FileSetStore& store, const std::filesystem::path& path)
{
  std::unordered_set<std::string> keys;
  push_path_forms(keys, path);
  std::vector<dirtoo::sets::FileSet> out;
  std::unordered_set<std::string> seen_ids;
  for (const auto& k : keys) {
    for (const auto& s : store.sets_for_path(k)) {
      if (seen_ids.insert(s.id).second) {
        out.push_back(s);
      }
    }
  }
  return out;
}

/// Parse a pure set:… expression (optional surrounding whitespace). Empty = not pure set.
[[nodiscard]] inline std::optional<std::string> pure_set_query(std::string_view expr)
{
  while (!expr.empty() && std::isspace(static_cast<unsigned char>(expr.front()))) {
    expr.remove_prefix(1);
  }
  while (!expr.empty() && std::isspace(static_cast<unsigned char>(expr.back()))) {
    expr.remove_suffix(1);
  }
  if (expr.size() < 5) {
    return std::nullopt;
  }
  // set:
  if (!(expr[0] == 's' || expr[0] == 'S') || !(expr[1] == 'e' || expr[1] == 'E')
      || !(expr[2] == 't' || expr[2] == 'T') || expr[3] != ':') {
    return std::nullopt;
  }
  // Pure: no spaces after set: (compound filters go through MatchFunc).
  const auto rest = expr.substr(4);
  for (char c : rest) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      return std::nullopt;
    }
  }
  if (rest.empty()) {
    return std::nullopt;
  }
  return std::string{rest};
}

} // namespace set_membership
} // namespace dirtoo::app

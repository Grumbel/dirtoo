// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/sets/file_set_store.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace dirtoo::app {
namespace set_membership {

/// Path string forms for membership lookup. Avoids weakly_canonical / equivalent
/// (those are disk-bound and were the set-filter latency source).
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

/// Preloaded membership index for one set (build once per filter pass).
struct MemberIndex {
  std::unordered_set<std::string> keys; ///< All path_key forms for members
  /// basename → parent path forms (for same-dir basename fallback)
  std::vector<std::pair<std::string, std::unordered_set<std::string>>> by_basename;
};

[[nodiscard]] inline MemberIndex build_member_index(dirtoo::sets::FileSetStore& store,
                                                    std::string_view set_id)
{
  MemberIndex idx;
  for (const auto& m : store.members(set_id)) {
    idx.keys.insert(m.path_key);
    push_path_forms(idx.keys, std::filesystem::path{m.path_key});
    const std::filesystem::path mp{m.path_key};
    const std::string base = mp.filename().string();
    if (!base.empty()) {
      std::unordered_set<std::string> parents;
      push_path_forms(parents, mp.parent_path());
      idx.by_basename.emplace_back(base, std::move(parents));
    }
  }
  return idx;
}

[[nodiscard]] inline bool path_in_index(const MemberIndex& idx, const std::filesystem::path& path)
{
  std::unordered_set<std::string> keys;
  push_path_forms(keys, path);
  for (const auto& k : keys) {
    if (idx.keys.contains(k)) {
      return true;
    }
  }
  // Same directory + same basename without filesystem::equivalent.
  const std::string base = path.filename().string();
  if (base.empty()) {
    return false;
  }
  std::unordered_set<std::string> parents;
  push_path_forms(parents, path.parent_path());
  for (const auto& [name, mem_parents] : idx.by_basename) {
    if (name != base) {
      continue;
    }
    for (const auto& p : parents) {
      if (mem_parents.contains(p)) {
        return true;
      }
    }
  }
  return false;
}

/// Convenience: load members once then test (prefer build_member_index in loops).
[[nodiscard]] inline bool path_in_set(dirtoo::sets::FileSetStore& store, std::string_view set_id,
                                      const std::filesystem::path& path)
{
  return path_in_index(build_member_index(store, set_id), path);
}

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
  if (!(expr[0] == 's' || expr[0] == 'S') || !(expr[1] == 'e' || expr[1] == 'E')
      || !(expr[2] == 't' || expr[2] == 'T') || expr[3] != ':') {
    return std::nullopt;
  }
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


/// If expr is one or more set:… terms joined by OR (and nothing else), return the queries.
[[nodiscard]] inline std::optional<std::vector<std::string>>
pure_set_or_queries(std::string_view expr)
{
  // Reuse pure_set_query for a single term; for OR, split on top-level OR.
  while (!expr.empty() && std::isspace(static_cast<unsigned char>(expr.front()))) {
    expr.remove_prefix(1);
  }
  while (!expr.empty() && std::isspace(static_cast<unsigned char>(expr.back()))) {
    expr.remove_suffix(1);
  }
  if (expr.empty()) {
    return std::nullopt;
  }
  if (auto one = pure_set_query(expr)) {
    return std::vector<std::string>{*one};
  }
  std::vector<std::string> parts;
  std::string cur;
  int depth = 0;
  for (std::size_t i = 0; i < expr.size(); ++i) {
    const char c = expr[i];
    if (c == '(') {
      ++depth;
      cur.push_back(c);
      continue;
    }
    if (c == ')') {
      depth = std::max(0, depth - 1);
      cur.push_back(c);
      continue;
    }
    if (depth == 0 && i + 1 < expr.size()
        && (c == 'O' || c == 'o') && (expr[i + 1] == 'R' || expr[i + 1] == 'r')) {
      const bool before_ok = (i == 0) || std::isspace(static_cast<unsigned char>(expr[i - 1]))
                             || expr[i - 1] == ')';
      const std::size_t j = i + 2;
      const bool after_ok =
          (j >= expr.size()) || std::isspace(static_cast<unsigned char>(expr[j])) || expr[j] == '(';
      if (before_ok && after_ok) {
        // trim cur
        std::string_view cv{cur};
        while (!cv.empty() && std::isspace(static_cast<unsigned char>(cv.front()))) {
          cv.remove_prefix(1);
        }
        while (!cv.empty() && std::isspace(static_cast<unsigned char>(cv.back()))) {
          cv.remove_suffix(1);
        }
        // strip one layer of parens
        if (cv.size() >= 2 && cv.front() == '(' && cv.back() == ')') {
          cv = cv.substr(1, cv.size() - 2);
        }
        if (auto q = pure_set_query(cv)) {
          parts.push_back(*q);
        } else {
          return std::nullopt;
        }
        cur.clear();
        i = j - 1;
        while (i + 1 < expr.size() && std::isspace(static_cast<unsigned char>(expr[i + 1]))) {
          ++i;
        }
        continue;
      }
    }
    cur.push_back(c);
  }
  std::string_view cv{cur};
  while (!cv.empty() && std::isspace(static_cast<unsigned char>(cv.front()))) {
    cv.remove_prefix(1);
  }
  while (!cv.empty() && std::isspace(static_cast<unsigned char>(cv.back()))) {
    cv.remove_suffix(1);
  }
  if (cv.size() >= 2 && cv.front() == '(' && cv.back() == ')') {
    cv = cv.substr(1, cv.size() - 2);
  }
  if (auto q = pure_set_query(cv)) {
    parts.push_back(*q);
  } else {
    return std::nullopt;
  }
  if (parts.size() < 2) {
    return std::nullopt; // single already handled
  }
  return parts;
}

} // namespace set_membership
} // namespace dirtoo::app

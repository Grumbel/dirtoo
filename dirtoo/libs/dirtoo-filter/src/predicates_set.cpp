// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

/// set: / in-set: predicates — path membership lookup; never hashes.

#include "dirtoo/filter/predicates.hpp"

#include "dirtoo/sets/file_set_store.hpp"

#include <cctype>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace dirtoo::filter {
namespace {

std::string lower_copy(std::string_view s)
{
  std::string out{s};
  for (char& c : out) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

/// Normalize a filesystem path into several string forms for DB lookup.
void push_path_forms(std::unordered_set<std::string>& out, const std::filesystem::path& p)
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

struct SetStoreHolder {
  dirtoo::sets::FileSetStore store;
  bool open = false;
  std::mutex mu;

  static SetStoreHolder& instance()
  {
    static SetStoreHolder inst;
    return inst;
  }

  bool ensure_open()
  {
    if (open && store.is_open()) {
      return true;
    }
    std::string err;
    open = store.open(dirtoo::sets::FileSetStore::default_path(), &err);
    return open;
  }
};

/// Resolve set: query → set id (exact id, unique prefix ≥8 chars, or unique label).
std::optional<std::string> resolve_set_id(dirtoo::sets::FileSetStore& store, std::string_view query)
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
          return std::nullopt; // ambiguous
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

class SetNameMatch : public MatchFunc {
public:
  explicit SetNameMatch(std::string query)
      : query_(std::move(query))
  {
    load_members();
  }

  bool matches(const FilterItem& item) const override
  {
    if (!resolved_ || member_keys_.empty()) {
      return false;
    }
    std::unordered_set<std::string> item_keys;
    push_path_forms(item_keys, item.path);
    for (const auto& k : item_keys) {
      if (member_keys_.contains(k)) {
        return true;
      }
    }
    // Filesystem equivalence (symlinks / relative vs absolute).
    for (const auto& mk : member_paths_) {
      std::error_code ec;
      if (std::filesystem::equivalent(item.path, mk, ec) && !ec) {
        return true;
      }
    }
    // Same directory + same basename (covers residual path-string mismatches).
    const auto item_parent = item.path.parent_path();
    const auto item_name = item.path.filename().string();
    if (item_name.empty()) {
      return false;
    }
    for (const auto& mk : member_paths_) {
      if (mk.filename().string() == item_name) {
        std::error_code ec;
        if (std::filesystem::equivalent(item_parent, mk.parent_path(), ec) && !ec) {
          return true;
        }
        // String compare parents after normalization.
        std::unordered_set<std::string> a;
        std::unordered_set<std::string> b;
        push_path_forms(a, item_parent);
        push_path_forms(b, mk.parent_path());
        for (const auto& x : a) {
          if (b.contains(x)) {
            return true;
          }
        }
      }
    }
    return false;
  }

private:
  void load_members()
  {
    auto& holder = SetStoreHolder::instance();
    std::lock_guard lock(holder.mu);
    if (!holder.ensure_open()) {
      return;
    }
    auto id = resolve_set_id(holder.store, query_);
    if (!id) {
      return;
    }
    resolved_ = true;
    set_id_ = *id;
    for (const auto& m : holder.store.members(*id)) {
      member_paths_.emplace_back(m.path_key);
      push_path_forms(member_keys_, std::filesystem::path{m.path_key});
      member_keys_.insert(m.path_key);
    }
  }

  std::string query_;
  bool resolved_ = false;
  std::string set_id_;
  std::unordered_set<std::string> member_keys_;
  std::vector<std::filesystem::path> member_paths_;
};

class InSetMatch : public MatchFunc {
public:
  explicit InSetMatch(bool want_any)
      : want_any_(want_any)
  {
  }

  bool matches(const FilterItem& item) const override
  {
    auto& holder = SetStoreHolder::instance();
    std::lock_guard lock(holder.mu);
    if (!holder.ensure_open()) {
      return want_any_ ? false : true;
    }
    std::unordered_set<std::string> keys;
    push_path_forms(keys, item.path);
    bool has = false;
    for (const auto& k : keys) {
      if (!holder.store.sets_for_path(k).empty()) {
        has = true;
        break;
      }
    }
    if (!has) {
      // equivalent scan is too heavy for in-set:yes over large dirs; path forms only.
    }
    return want_any_ ? has : !has;
  }

private:
  bool want_any_;
};

} // namespace

MatchFuncPtr make_set(std::string_view arg)
{
  std::string query{arg};
  while (!query.empty() && std::isspace(static_cast<unsigned char>(query.front()))) {
    query.erase(query.begin());
  }
  while (!query.empty() && std::isspace(static_cast<unsigned char>(query.back()))) {
    query.pop_back();
  }
  if (query.empty()) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<SetNameMatch>(std::move(query));
}

MatchFuncPtr make_in_set(std::string_view arg)
{
  const std::string a = lower_copy(arg);
  if (a == "yes" || a == "true" || a == "1" || a == "any" || a.empty()) {
    return std::make_shared<InSetMatch>(true);
  }
  if (a == "no" || a == "false" || a == "0" || a == "none") {
    return std::make_shared<InSetMatch>(false);
  }
  return std::make_shared<AlwaysFalse>();
}

} // namespace dirtoo::filter

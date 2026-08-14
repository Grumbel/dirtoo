// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

/// set: / in-set: predicates — path membership lookup; never hashes.

#include "dirtoo/filter/predicates.hpp"

#include "dirtoo/sets/file_set_store.hpp"

#include <cctype>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace dirtoo::filter {
namespace {

struct SetLookup {
  dirtoo::sets::FileSetStore store;
  bool open = false;
  mutable std::mutex mu;

  static SetLookup& instance()
  {
    static SetLookup inst;
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

  /// Candidate path keys (absolute + as stored) so listing paths match DB rows.
  [[nodiscard]] static std::vector<std::string> path_keys(const FilterItem& item)
  {
    std::vector<std::string> keys;
    const std::string path_str = item.path.string();
    if (path_str.empty()) {
      return keys;
    }
    auto push_unique = [&](std::string k) {
      if (k.empty()) {
        return;
      }
      for (const auto& e : keys) {
        if (e == k) {
          return;
        }
      }
      keys.push_back(std::move(k));
    };
    push_unique(path_str);
    if (path_str.find("://") != std::string::npos
        || path_str.find("//archive") != std::string::npos) {
      return keys;
    }
    std::error_code ec;
    const auto abs = std::filesystem::absolute(item.path, ec);
    if (!ec) {
      push_unique(abs.lexically_normal().string());
      push_unique(abs.string());
    }
    const auto weak = std::filesystem::weakly_canonical(item.path, ec);
    if (!ec) {
      push_unique(weak.string());
    }
    return keys;
  }

  [[nodiscard]] std::vector<dirtoo::sets::FileSet> sets_for(const FilterItem& item)
  {
    std::lock_guard<std::mutex> lock(mu);
    if (!ensure_open()) {
      return {};
    }
    for (const auto& key : path_keys(item)) {
      auto sets = store.sets_for_path(key);
      if (!sets.empty()) {
        return sets;
      }
    }
    return {};
  }

  [[nodiscard]] bool in_set(const FilterItem& item, std::string_view query)
  {
    std::lock_guard<std::mutex> lock(mu);
    if (!ensure_open() || query.empty()) {
      return false;
    }
    const auto keys = path_keys(item);

    auto path_in = [&](std::string_view set_id) {
      for (const auto& key : keys) {
        if (store.contains(set_id, key)) {
          return true;
        }
      }
      return false;
    };

    // Exact id.
    if (path_in(query)) {
      return true;
    }
    // Unique id prefix (QuickFilter may show short ids).
    if (query.size() >= 8) {
      std::vector<std::string> prefix_hits;
      for (const auto& s : store.list_sets()) {
        if (s.id.size() >= query.size()
            && s.id.compare(0, query.size(), query) == 0) {
          prefix_hits.push_back(s.id);
        }
      }
      if (prefix_hits.size() == 1 && path_in(prefix_hits.front())) {
        return true;
      }
    }
    // Label match.
    for (const auto& s : store.list_sets()) {
      if (s.label == query && path_in(s.id)) {
        return true;
      }
    }
    return false;
  }
};

std::string lower_copy(std::string_view s)
{
  std::string out{s};
  for (char& c : out) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

class SetNameMatch : public MatchFunc {
public:
  explicit SetNameMatch(std::string query)
      : query_(std::move(query))
  {
  }

  bool matches(const FilterItem& item) const override
  {
    if (query_.empty()) {
      return false;
    }
    return SetLookup::instance().in_set(item, query_);
  }

private:
  std::string query_;
};

class InSetMatch : public MatchFunc {
public:
  explicit InSetMatch(bool want_any)
      : want_any_(want_any)
  {
  }

  bool matches(const FilterItem& item) const override
  {
    const bool has = !SetLookup::instance().sets_for(item).empty();
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

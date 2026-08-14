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
    static std::once_flag once;
    std::call_once(once, [] {
      std::string err;
      inst.open = inst.store.open(dirtoo::sets::FileSetStore::default_path(), &err);
    });
    return inst;
  }

  [[nodiscard]] std::string path_key(const FilterItem& item) const
  {
    const std::string path_str = item.path.string();
    if (path_str.find("://") != std::string::npos
        || path_str.find("//archive") != std::string::npos) {
      return path_str;
    }
    std::error_code ec;
    const auto abs = std::filesystem::absolute(item.path, ec);
    if (ec) {
      return path_str;
    }
    return abs.lexically_normal().string();
  }

  [[nodiscard]] std::vector<dirtoo::sets::FileSet> sets_for(const FilterItem& item) const
  {
    std::lock_guard<std::mutex> lock(mu);
    if (!open) {
      return {};
    }
    return store.sets_for_path(path_key(item));
  }

  [[nodiscard]] bool in_set(const FilterItem& item, std::string_view query) const
  {
    std::lock_guard<std::mutex> lock(mu);
    if (!open || query.empty()) {
      return false;
    }
    const std::string key = path_key(item);
    // Exact id.
    if (store.contains(query, key)) {
      return true;
    }
    // Label match (any set with that label containing path).
    for (const auto& s : store.list_sets()) {
      if (s.label == query && store.contains(s.id, key)) {
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

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

} // namespace

MatchFuncPtr make_set(std::string_view arg)
{
  const std::string query{arg};
  return [query](const FilterItem& item) -> bool {
    if (query.empty()) {
      return false;
    }
    return SetLookup::instance().in_set(item, query);
  };
}

MatchFuncPtr make_in_set(std::string_view arg)
{
  const std::string a = lower_copy(arg);
  const bool want = (a.empty() || a == "yes" || a == "true" || a == "1");
  const bool want_no = (a == "no" || a == "false" || a == "0");
  return [want, want_no](const FilterItem& item) -> bool {
    const bool has = !SetLookup::instance().sets_for(item).empty();
    if (want_no) {
      return !has;
    }
    return want ? has : has;
  };
}

} // namespace dirtoo::filter

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

/// tag: / tagged: predicates — lookup only; never hash files.

#include "dirtoo/filter/predicates.hpp"

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/tags/tag_store.hpp"

#include <cctype>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace dirtoo::filter {
namespace {

/// Shared read-only lookup for filter matches. Serialized: SQLite handles are
/// not safe for concurrent use on one connection (FilterWorker + GUI).
struct TagLookup {
  dirtoo::hash::ChecksumStore checksums;
  dirtoo::tags::TagStore tags;
  bool open = false;
  mutable std::mutex mu;

  static TagLookup& instance()
  {
    static TagLookup inst;
    static std::once_flag once;
    std::call_once(once, [] {
      std::string err;
      inst.open = inst.checksums.open(dirtoo::hash::ChecksumStore::default_path(), &err)
                  && inst.tags.open(dirtoo::tags::TagStore::default_path(), &err);
    });
    return inst;
  }

  [[nodiscard]] std::string path_key(const FilterItem& item) const
  {
    const std::string path_str = item.path.string();
    // Archive members use Location URL as path; keep key stable.
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

  [[nodiscard]] std::vector<std::string> tags_for(const FilterItem& item) const
  {
    std::lock_guard<std::mutex> lock(mu);
    if (!open) {
      return {};
    }
    return tags.tags_for_path(checksums, path_key(item));
  }

  [[nodiscard]] bool has_checksum(const FilterItem& item) const
  {
    std::lock_guard<std::mutex> lock(mu);
    if (!open) {
      return false;
    }
    // Full digests only — quick: keys are never treated as tagged identity.
    return checksums.has_full(path_key(item));
  }

  [[nodiscard]] bool has_quick_checksum(const FilterItem& item) const
  {
    std::lock_guard<std::mutex> lock(mu);
    if (!open) {
      return false;
    }
    return checksums.has_quick(path_key(item));
  }
};

class TagNameMatch : public MatchFunc {
public:
  explicit TagNameMatch(std::string query)
      : query_(std::move(query))
  {
  }
  bool matches(const FilterItem& item) const override
  {
    if (query_.empty()) {
      return false;
    }
    const auto tags = TagLookup::instance().tags_for(item);
    for (const auto& t : tags) {
      if (dirtoo::tags::tag_name_matches(t, query_)) {
        return true;
      }
    }
    return false;
  }

private:
  std::string query_;
};

class TaggedMatch : public MatchFunc {
public:
  explicit TaggedMatch(bool want_any)
      : want_any_(want_any)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    const bool has = !TagLookup::instance().tags_for(item).empty();
    return want_any_ ? has : !has;
  }

private:
  bool want_any_;
};

class ChecksummedMatch : public MatchFunc {
public:
  enum class Kind { Full, Quick };
  ChecksummedMatch(bool want_present, Kind kind = Kind::Full)
      : want_present_(want_present)
      , kind_(kind)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    const bool has = (kind_ == Kind::Quick) ? TagLookup::instance().has_quick_checksum(item)
                                            : TagLookup::instance().has_checksum(item);
    return want_present_ ? has : !has;
  }

private:
  bool want_present_;
  Kind kind_;
};

} // namespace

MatchFuncPtr make_tag(std::string_view tag_name)
{
  // Preserve trailing '*' for glob; normalize the rest via the same rules as
  // tag definitions (lowercase, allow namespace ':').
  std::string raw(tag_name);
  bool glob = false;
  if (!raw.empty() && raw.back() == '*') {
    glob = true;
    raw.pop_back();
  }
  std::string norm = dirtoo::tags::normalize_tag_name(raw);
  if (norm.empty() && !raw.empty()) {
    // normalize rejected the name
    return std::make_shared<AlwaysFalse>();
  }
  if (glob) {
    norm.push_back('*');
  }
  if (norm.empty()) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<TagNameMatch>(std::move(norm));
}

MatchFuncPtr make_tagged(std::string_view arg)
{
  std::string a(arg);
  for (char& c : a) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (a == "yes" || a == "true" || a == "1" || a == "any") {
    return std::make_shared<TaggedMatch>(true);
  }
  if (a == "no" || a == "false" || a == "0" || a == "none") {
    return std::make_shared<TaggedMatch>(false);
  }
  if (a.empty()) {
    return std::make_shared<TaggedMatch>(true);
  }
  return std::make_shared<AlwaysFalse>();
}

MatchFuncPtr make_checksummed(std::string_view arg)
{
  std::string a(arg);
  for (char& c : a) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (a == "yes" || a == "true" || a == "1" || a == "any" || a == "full") {
    return std::make_shared<ChecksummedMatch>(true, ChecksummedMatch::Kind::Full);
  }
  if (a == "no" || a == "false" || a == "0" || a == "none") {
    return std::make_shared<ChecksummedMatch>(false, ChecksummedMatch::Kind::Full);
  }
  if (a == "quick" || a == "sample") {
    return std::make_shared<ChecksummedMatch>(true, ChecksummedMatch::Kind::Quick);
  }
  return std::make_shared<AlwaysFalse>();
}


} // namespace dirtoo::filter

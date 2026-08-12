// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

/// tag: / tagged: predicates — lookup only; never hash files.

#include "dirtoo/filter/predicates.hpp"

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/tags/tag_store.hpp"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace dirtoo::filter {
namespace {

struct TagLookup {
  dirtoo::hash::ChecksumStore checksums;
  dirtoo::tags::TagStore tags;
  bool open = false;

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
    std::error_code ec;
    const auto abs = std::filesystem::absolute(item.path, ec);
    if (ec) {
      return item.path.string();
    }
    return abs.lexically_normal().string();
  }

  [[nodiscard]] std::vector<std::string> tags_for(const FilterItem& item) const
  {
    if (!open) {
      return {};
    }
    return tags.tags_for_path(checksums, path_key(item));
  }
};

class TagNameMatch : public MatchFunc {
public:
  explicit TagNameMatch(std::string name)
      : name_(std::move(name))
  {
  }
  bool matches(const FilterItem& item) const override
  {
    if (name_.empty()) {
      return false;
    }
    const auto tags = TagLookup::instance().tags_for(item);
    for (const auto& t : tags) {
      if (t == name_) {
        return true;
      }
    }
    return false;
  }

private:
  std::string name_;
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

} // namespace

MatchFuncPtr make_tag(std::string_view tag_name)
{
  const std::string norm = dirtoo::tags::normalize_tag_name(tag_name);
  if (norm.empty()) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<TagNameMatch>(norm);
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
  // bare tagged: → any tag
  if (a.empty()) {
    return std::make_shared<TaggedMatch>(true);
  }
  return std::make_shared<AlwaysFalse>();
}

} // namespace dirtoo::filter

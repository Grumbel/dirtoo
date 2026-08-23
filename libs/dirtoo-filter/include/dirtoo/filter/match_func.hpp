// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/filter/filter_item.hpp"

#include <memory>
#include <vector>

namespace dirtoo::filter {

class MatchFunc {
public:
  virtual ~MatchFunc() = default;
  [[nodiscard]] virtual bool matches(const FilterItem& item) const = 0;
};

using MatchFuncPtr = std::shared_ptr<MatchFunc>;

class AlwaysTrue : public MatchFunc {
public:
  bool matches(const FilterItem&) const override { return true; }
};

class AlwaysFalse : public MatchFunc {
public:
  bool matches(const FilterItem&) const override { return false; }
};

class AndMatch : public MatchFunc {
public:
  explicit AndMatch(std::vector<MatchFuncPtr> parts)
      : parts_(std::move(parts))
  {
  }
  bool matches(const FilterItem& item) const override
  {
    for (const auto& p : parts_) {
      if (p == nullptr || !p->matches(item)) {
        return false;
      }
    }
    return true;
  }

private:
  std::vector<MatchFuncPtr> parts_;
};

class OrMatch : public MatchFunc {
public:
  explicit OrMatch(std::vector<MatchFuncPtr> parts)
      : parts_(std::move(parts))
  {
  }
  bool matches(const FilterItem& item) const override
  {
    for (const auto& p : parts_) {
      if (p != nullptr && p->matches(item)) {
        return true;
      }
    }
    return false;
  }

private:
  std::vector<MatchFuncPtr> parts_;
};

class NotMatch : public MatchFunc {
public:
  explicit NotMatch(MatchFuncPtr inner)
      : inner_(std::move(inner))
  {
  }
  bool matches(const FilterItem& item) const override
  {
    return inner_ == nullptr || !inner_->matches(item);
  }

private:
  MatchFuncPtr inner_;
};

} // namespace dirtoo::filter

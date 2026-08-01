// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace dirtoo::fs {

/// URI-like location. Phase 1 supports the file:// protocol only.
/// Archive payload stacks (file.rar//rar:...) are deferred.
class Location {
public:
  Location() = default;

  [[nodiscard]] static Location from_path(const std::filesystem::path& path);
  [[nodiscard]] static Location from_url(std::string_view url);
  [[nodiscard]] static Location from_human(std::string_view text);

  [[nodiscard]] std::string as_url() const;
  [[nodiscard]] std::filesystem::path as_path() const;

  [[nodiscard]] Location parent() const;
  [[nodiscard]] Location join(std::string_view child) const;

  [[nodiscard]] std::string basename() const;
  [[nodiscard]] std::string dirname() const;

  [[nodiscard]] bool empty() const noexcept { return path_.empty(); }

  [[nodiscard]] bool operator==(const Location&) const = default;
  [[nodiscard]] auto operator<=>(const Location&) const = default;

private:
  explicit Location(std::filesystem::path path);

  std::string protocol_{"file"};
  std::filesystem::path path_;
};

} // namespace dirtoo::fs

namespace std {
template <>
struct hash<dirtoo::fs::Location> {
  size_t operator()(const dirtoo::fs::Location& loc) const noexcept
  {
    return hash<string>{}(loc.as_url());
  }
};
} // namespace std

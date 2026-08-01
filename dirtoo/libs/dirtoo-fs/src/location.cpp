// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/fs/location.hpp"

#include <stdexcept>

namespace dirtoo::fs {

Location::Location(std::filesystem::path path)
    : path_(std::filesystem::weakly_canonical(path.is_absolute()
                                                  ? path
                                                  : std::filesystem::absolute(path)))
{
}

Location Location::from_path(const std::filesystem::path& path)
{
  return Location{path};
}

Location Location::from_url(std::string_view url)
{
  constexpr std::string_view prefix = "file://";
  if (!url.starts_with(prefix)) {
    throw std::invalid_argument("only file:// URLs are supported in this phase");
  }
  // Minimal percent-decoding for spaces; full URL decode can come later.
  std::string path_str{url.substr(prefix.size())};
  for (std::string::size_type pos = 0; (pos = path_str.find("%20", pos)) != std::string::npos;) {
    path_str.replace(pos, 3, " ");
  }
  return Location{std::filesystem::path{path_str}};
}

Location Location::from_human(std::string_view text)
{
  if (text.empty() || text == ".") {
    return from_path(std::filesystem::current_path());
  }
  if (text.starts_with("file://")) {
    return from_url(text);
  }
  return from_path(std::filesystem::path{std::string{text}});
}

std::string Location::as_url() const
{
  std::string path = path_.string();
  std::string out = "file://";
  for (char c : path) {
    if (c == ' ') {
      out += "%20";
    } else {
      out += c;
    }
  }
  return out;
}

std::filesystem::path Location::as_path() const
{
  return path_;
}

Location Location::parent() const
{
  if (path_.has_parent_path() && path_ != path_.root_path()) {
    return Location{path_.parent_path()};
  }
  return Location{path_.root_path().empty() ? std::filesystem::path{"/"} : path_.root_path()};
}

Location Location::join(std::string_view child) const
{
  return Location{path_ / std::filesystem::path{std::string{child}}};
}

std::string Location::basename() const
{
  return path_.filename().string();
}

std::string Location::dirname() const
{
  return path_.parent_path().string();
}

} // namespace dirtoo::fs

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "bookmarks.hpp"

#include <QStandardPaths>

#include <algorithm>
#include <fstream>

namespace dirtoo::app {

Bookmarks::Bookmarks(std::filesystem::path file)
    : path_(std::move(file))
{
}

std::filesystem::path Bookmarks::default_path()
{
  const QString cfg = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  std::filesystem::path dir = cfg.toStdString();
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir / "bookmarks.txt";
}

std::vector<fs::Location> Bookmarks::entries() const
{
  std::vector<fs::Location> result;
  std::ifstream in(path_);
  if (!in) {
    return result;
  }
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    try {
      // from_human accepts file:// / archive:// and bare path//archive: forms.
      result.push_back(fs::Location::from_human(line));
    } catch (...) {
      // ignore bad lines
    }
  }
  // Preserve file order (append order). Dedup by URL keeping first occurrence.
  std::vector<fs::Location> unique;
  unique.reserve(result.size());
  for (const auto& loc : result) {
    const auto url = loc.as_url();
    bool seen = false;
    for (const auto& u : unique) {
      if (u.as_url() == url) {
        seen = true;
        break;
      }
    }
    if (!seen) {
      unique.push_back(loc);
    }
  }
  return unique;
}

bool Bookmarks::contains(const fs::Location& location) const
{
  const auto url = location.as_url();
  for (const auto& e : entries()) {
    if (e.as_url() == url) {
      return true;
    }
  }
  return false;
}

void Bookmarks::write_all(const std::vector<fs::Location>& entries) const
{
  if (auto parent = path_.parent_path(); !parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
  }
  std::ofstream out(path_, std::ios::trunc);
  for (const auto& e : entries) {
    out << e.as_url() << '\n';
  }
}

void Bookmarks::append(const fs::Location& location)
{
  auto list = entries();
  const auto url = location.as_url();
  for (const auto& e : list) {
    if (e.as_url() == url) {
      return;
    }
  }
  list.push_back(location);
  write_all(list);
}

void Bookmarks::remove(const fs::Location& location)
{
  auto list = entries();
  const auto url = location.as_url();
  list.erase(std::remove_if(list.begin(), list.end(),
                            [&](const fs::Location& e) { return e.as_url() == url; }),
             list.end());
  write_all(list);
}

bool Bookmarks::toggle(const fs::Location& location)
{
  if (contains(location)) {
    remove(location);
    return false;
  }
  append(location);
  return true;
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/media_probe.hpp"

#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace {

void usage()
{
  std::cerr << "usage: dt-mediainfo [options] <path> [path…]\n"
               "  -p, --println FMT   Print FMT then newline (Python -p)\n"
               "  -P, --print FMT     Print FMT without newline (Python -P)\n"
               "  -h, --help\n\n"
               "Placeholders: {filename} {width} {height} {resolution}\n"
               "  {duration} {hours} {minutes} {seconds} {framerate} {pages} {file_count}\n";
}

struct Fields {
  std::string filename;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint64_t duration_ms = 0;
  double framerate = 0;
  std::uint64_t pages = 0;
  std::uint64_t file_count = 0;
};

Fields from_info(const std::filesystem::path& path, const dirtoo::filter::MediaInfo& info)
{
  Fields f;
  f.filename = path.filename().string();
  if (info.width) f.width = *info.width;
  if (info.height) f.height = *info.height;
  if (info.duration_ms) f.duration_ms = *info.duration_ms;
  if (info.framerate) f.framerate = *info.framerate;
  if (info.pages) f.pages = *info.pages;
  if (info.file_count) f.file_count = *info.file_count;
  return f;
}

std::string replace_all(std::string s, std::string_view key, std::string_view value)
{
  for (;;) {
    const auto pos = s.find(key);
    if (pos == std::string::npos) break;
    s.replace(pos, key.size(), value);
  }
  return s;
}

std::string format_line(const std::string& fmt, const Fields& f)
{
  const auto total_s = static_cast<int>(f.duration_ms / 1000);
  const int hours = total_s / 3600;
  const int minutes = (total_s % 3600) / 60;
  const int seconds = total_s % 60;
  std::ostringstream res;
  res << f.width << 'x' << f.height;
  std::ostringstream fps;
  fps << std::fixed << std::setprecision(2) << f.framerate;
  std::string out = fmt;
  out = replace_all(out, "{filename}", f.filename);
  out = replace_all(out, "{width}", std::to_string(f.width));
  out = replace_all(out, "{height}", std::to_string(f.height));
  out = replace_all(out, "{resolution}", res.str());
  out = replace_all(out, "{duration}", std::to_string(f.duration_ms / 1000.0));
  out = replace_all(out, "{hours}", std::to_string(hours));
  out = replace_all(out, "{minutes}", std::to_string(minutes));
  out = replace_all(out, "{seconds}", std::to_string(seconds));
  out = replace_all(out, "{framerate}", fps.str());
  out = replace_all(out, "{pages}", std::to_string(f.pages));
  out = replace_all(out, "{file_count}", std::to_string(f.file_count));
  return out;
}

const char* kDefaultFmt =
    "{hours}h:{minutes}m:{seconds}s  {framerate}fps  {resolution}  {filename}\n";

} // namespace

int main(int argc, char* argv[])
{
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--version" || std::string_view(argv[i]) == "-V") {
      std::cout << "dirtoo " DIRTOO_VERSION "\n";
      return 0;
    }
  }

  std::string fmt = kDefaultFmt;
  bool add_newline = false;
  std::vector<std::filesystem::path> paths;

  for (int i = 1; i < argc; ++i) {
    const std::string_view a{argv[i]};
    if (a == "-h" || a == "--help") {
      usage();
      return 0;
    }
    if (a == "-p" || a == "--println") {
      if (i + 1 >= argc) {
        std::cerr << "dt-mediainfo: missing FMT\n";
        return 2;
      }
      fmt = argv[++i];
      add_newline = true;
      continue;
    }
    if (a == "-P" || a == "--print") {
      if (i + 1 >= argc) {
        std::cerr << "dt-mediainfo: missing FMT\n";
        return 2;
      }
      fmt = argv[++i];
      add_newline = false;
      continue;
    }
    if (a.starts_with('-')) {
      std::cerr << "dt-mediainfo: unknown option: " << a << '\n';
      usage();
      return 2;
    }
    paths.emplace_back(argv[i]);
  }

  if (paths.empty()) {
    usage();
    return 2;
  }

  int failures = 0;
  for (const auto& path : paths) {
    auto info = dirtoo::filter::probe_media(path);
    if (!info) {
      std::cerr << path.string() << ": no media metadata\n";
      ++failures;
      continue;
    }
    std::cout << format_line(fmt, from_info(path, *info));
    if (add_newline) {
      std::cout << '\n';
    }
  }
  return failures == 0 ? 0 : 1;
}

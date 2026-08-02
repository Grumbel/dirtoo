// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/media_probe.hpp"
#include "json_util.hpp"

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
               "  -j, --json          Machine-readable JSON array\n"
               "  -p, --println FMT   Print FMT then newline\n"
               "  -P, --print FMT     Print FMT without newline\n"
               "  -h, --help\n\n"
               "Placeholders: {filename} {width} {height} {resolution}\n"
               "  {duration} {hours} {minutes} {seconds} {framerate} {pages} {file_count}\n";
}

struct Fields {
  std::string path;
  std::string filename;
  bool has_width = false;
  bool has_height = false;
  bool has_duration = false;
  bool has_framerate = false;
  bool has_pages = false;
  bool has_file_count = false;
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
  f.path = path.string();
  f.filename = path.filename().string();
  if (info.width) {
    f.has_width = true;
    f.width = *info.width;
  }
  if (info.height) {
    f.has_height = true;
    f.height = *info.height;
  }
  if (info.duration_ms) {
    f.has_duration = true;
    f.duration_ms = *info.duration_ms;
  }
  if (info.framerate) {
    f.has_framerate = true;
    f.framerate = *info.framerate;
  }
  if (info.pages) {
    f.has_pages = true;
    f.pages = *info.pages;
  }
  if (info.file_count) {
    f.has_file_count = true;
    f.file_count = *info.file_count;
  }
  return f;
}

std::string replace_all(std::string s, std::string_view key, std::string_view value)
{
  for (;;) {
    const auto pos = s.find(key);
    if (pos == std::string::npos) {
      break;
    }
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

void write_json_object(std::ostream& out, const Fields& f)
{
  out << '{';
  bool first = true;
  dtjson::write_kv_string(out, "path", f.path, first);
  dtjson::write_kv_string(out, "filename", f.filename, first);
  if (f.has_width) {
    dtjson::write_kv_uint(out, "width", f.width, first);
  }
  if (f.has_height) {
    dtjson::write_kv_uint(out, "height", f.height, first);
  }
  if (f.has_width || f.has_height) {
    std::ostringstream res;
    res << f.width << 'x' << f.height;
    dtjson::write_kv_string(out, "resolution", res.str(), first);
  }
  if (f.has_duration) {
    dtjson::write_kv_uint(out, "duration_ms", f.duration_ms, first);
    dtjson::write_kv_double(out, "duration_s", f.duration_ms / 1000.0, first);
  }
  if (f.has_framerate) {
    dtjson::write_kv_double(out, "framerate", f.framerate, first);
  }
  if (f.has_pages) {
    dtjson::write_kv_uint(out, "pages", f.pages, first);
  }
  if (f.has_file_count) {
    dtjson::write_kv_uint(out, "file_count", f.file_count, first);
  }
  out << '}';
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
  bool json = false;
  std::vector<std::filesystem::path> paths;

  for (int i = 1; i < argc; ++i) {
    const std::string_view a{argv[i]};
    if (a == "-h" || a == "--help") {
      usage();
      return 0;
    }
    if (a == "-j" || a == "--json") {
      json = true;
      continue;
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
  if (json) {
    std::cout << '[';
    bool first_obj = true;
    for (const auto& path : paths) {
      auto info = dirtoo::filter::probe_media(path);
      if (!info) {
        std::cerr << path.string() << ": no media metadata\n";
        ++failures;
        continue;
      }
      if (!first_obj) {
        std::cout << ',';
      }
      first_obj = false;
      write_json_object(std::cout, from_info(path, *info));
    }
    std::cout << "]\n";
    return failures == 0 ? 0 : 1;
  }

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

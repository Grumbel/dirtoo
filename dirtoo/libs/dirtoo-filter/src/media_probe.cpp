// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/media_probe.hpp"

#include <array>
#include <cstdio>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

namespace dirtoo::filter {
namespace {

std::mutex g_cache_mutex;
std::map<std::string, std::optional<MediaInfo>> g_cache;

std::string find_ffprobe()
{
  if (const char* env = std::getenv("DIRTOO_FFPROBE"); env != nullptr && env[0] != '\0') {
    return env;
  }
  return "ffprobe";
}

// Minimal popen capture (Qt-free).
std::optional<std::string> run_capture(const std::string& cmd)
{
  FILE* pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr) {
    return std::nullopt;
  }
  std::string out;
  std::array<char, 512> buf{};
  while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
    out += buf.data();
  }
  const int rc = pclose(pipe);
  if (rc != 0 && out.empty()) {
    return std::nullopt;
  }
  return out;
}

std::optional<double> parse_double(std::string_view s)
{
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
    s.remove_prefix(1);
  }
  if (s.empty()) {
    return std::nullopt;
  }
  try {
    return std::stod(std::string{s});
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<double> parse_frame_rate(std::string_view s)
{
  // "30/1" or "29.97"
  const auto slash = s.find('/');
  if (slash == std::string_view::npos) {
    return parse_double(s);
  }
  const auto num = parse_double(s.substr(0, slash));
  const auto den = parse_double(s.substr(slash + 1));
  if (!num || !den || *den == 0.0) {
    return std::nullopt;
  }
  return *num / *den;
}

MediaInfo parse_ffprobe_output(const std::string& text)
{
  MediaInfo info;
  std::string_view view{text};
  while (!view.empty()) {
    const auto nl = view.find('\n');
    std::string_view line = nl == std::string_view::npos ? view : view.substr(0, nl);
    if (nl == std::string_view::npos) {
      view = {};
    } else {
      view.remove_prefix(nl + 1);
    }
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
      line.remove_suffix(1);
    }
    const auto eq = line.find('=');
    if (eq == std::string_view::npos) {
      continue;
    }
    const auto key = line.substr(0, eq);
    const auto val = line.substr(eq + 1);
    if (key == "width") {
      if (auto v = parse_double(val)) {
        info.width = static_cast<std::uint32_t>(*v);
      }
    } else if (key == "height") {
      if (auto v = parse_double(val)) {
        info.height = static_cast<std::uint32_t>(*v);
      }
    } else if (key == "duration") {
      if (auto v = parse_double(val); v && *v >= 0) {
        info.duration_ms = static_cast<std::uint64_t>(*v * 1000.0);
      }
    } else if (key == "r_frame_rate" || key == "avg_frame_rate") {
      if (!info.framerate) {
        info.framerate = parse_frame_rate(val);
      }
    }
  }
  return info;
}

} // namespace

std::optional<double> parse_duration_seconds(std::string_view text)
{
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
    text.remove_prefix(1);
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
    text.remove_suffix(1);
  }
  if (text.empty()) {
    return std::nullopt;
  }

  // Plain number (seconds)
  bool pure_number = true;
  for (char c : text) {
    if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.')) {
      pure_number = false;
      break;
    }
  }
  if (pure_number) {
    return parse_double(text);
  }

  // h:mm:ss or mm:ss
  if (text.find(':') != std::string_view::npos) {
    int parts[3] = {0, 0, 0};
    int n = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size() && n < 3; ++i) {
      if (i == text.size() || text[i] == ':') {
        auto seg = text.substr(start, i - start);
        if (auto v = parse_double(seg)) {
          parts[n++] = static_cast<int>(*v);
        } else {
          return std::nullopt;
        }
        start = i + 1;
      }
    }
    if (n == 2) {
      return parts[0] * 60.0 + parts[1];
    }
    if (n == 3) {
      return parts[0] * 3600.0 + parts[1] * 60.0 + parts[2];
    }
    return std::nullopt;
  }

  // 1h2m3s / 2m / 90s
  double total = 0;
  std::size_t i = 0;
  bool any = false;
  while (i < text.size()) {
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
      ++i;
    }
    if (i >= text.size()) {
      break;
    }
    std::size_t j = i;
    while (j < text.size()
           && (std::isdigit(static_cast<unsigned char>(text[j])) || text[j] == '.')) {
      ++j;
    }
    if (j == i) {
      return std::nullopt;
    }
    auto num = parse_double(text.substr(i, j - i));
    if (!num) {
      return std::nullopt;
    }
    if (j >= text.size()) {
      total += *num; // bare trailing number as seconds
      any = true;
      break;
    }
    const char unit = static_cast<char>(std::tolower(static_cast<unsigned char>(text[j])));
    if (unit == 'h') {
      total += *num * 3600.0;
    } else if (unit == 'm') {
      total += *num * 60.0;
    } else if (unit == 's') {
      total += *num;
    } else {
      return std::nullopt;
    }
    any = true;
    i = j + 1;
  }
  return any ? std::optional<double>{total} : std::nullopt;
}

void clear_media_probe_cache()
{
  std::lock_guard lock(g_cache_mutex);
  g_cache.clear();
}

std::optional<MediaInfo> probe_media(const std::filesystem::path& path)
{
  const std::string key = path.string();
  {
    std::lock_guard lock(g_cache_mutex);
    if (const auto it = g_cache.find(key); it != g_cache.end()) {
      return it->second;
    }
  }

  const std::string ffprobe = find_ffprobe();
  // Stream + format entries; nw=1 prints key=value lines without section headers.
  std::string cmd = ffprobe + " -v error -show_entries "
                    "stream=width,height,r_frame_rate,avg_frame_rate:format=duration "
                    "-of default=noprint_wrappers=1:nokey=0 ";
  // Quote path for the shell.
  cmd += '\'';
  for (char c : key) {
    if (c == '\'') {
      cmd += "'\\''";
    } else {
      cmd += c;
    }
  }
  cmd += '\'';

  auto out = run_capture(cmd);
  std::optional<MediaInfo> result;
  if (out) {
    auto info = parse_ffprobe_output(*out);
    if (info.width || info.height || info.duration_ms || info.framerate) {
      result = info;
    }
  }

  {
    std::lock_guard lock(g_cache_mutex);
    g_cache[key] = result;
  }
  return result;
}

} // namespace dirtoo::filter

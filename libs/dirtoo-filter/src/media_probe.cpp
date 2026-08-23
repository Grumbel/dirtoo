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


namespace {

bool path_ends_with_ci(const std::string& name, std::string_view suffix)
{
  if (name.size() < suffix.size()) {
    return false;
  }
  for (std::size_t i = 0; i < suffix.size(); ++i) {
    const auto a = static_cast<unsigned char>(name[name.size() - suffix.size() + i]);
    const auto b = static_cast<unsigned char>(suffix[i]);
    if (std::tolower(a) != std::tolower(b)) {
      return false;
    }
  }
  return true;
}

std::string quote_path(const std::string& key)
{
  std::string out = "'";
  for (char c : key) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out += c;
    }
  }
  out += "'";
  return out;
}

void enrich_pages_and_filecount(const std::filesystem::path& path, MediaInfo& info)
{
  const std::string key = path.string();
  const std::string name = path.filename().string();
  if (path_ends_with_ci(name, ".pdf") && !info.pages) {
    const char* pdfinfo = std::getenv("DIRTOO_PDFINFO");
    const std::string tool = (pdfinfo && pdfinfo[0]) ? pdfinfo : "pdfinfo";
    if (auto out = run_capture(tool + " " + quote_path(key) + " 2>/dev/null")) {
      for (std::size_t i = 0; i + 6 < out->size(); ++i) {
        if ((*out)[i] == 'P' && out->compare(i, 6, "Pages:") == 0) {
          std::size_t j = i + 6;
          while (j < out->size() && std::isspace(static_cast<unsigned char>((*out)[j]))) {
            ++j;
          }
          try {
            info.pages = static_cast<std::uint64_t>(std::stoull(out->substr(j)));
          } catch (...) {
          }
          break;
        }
      }
    }
  }
  if (!info.file_count) {
    static const char* kArch[] = {".zip", ".tar", ".tgz", ".7z", ".rar", ".cbz", ".cbr", ".jar",
                                  ".apk", ".tar.gz", ".tar.bz2", ".tar.xz"};
    bool is_arch = false;
    for (const char* s : kArch) {
      if (path_ends_with_ci(name, s)) {
        is_arch = true;
        break;
      }
    }
    if (is_arch) {
      if (auto out = run_capture("bsdtar -tf " + quote_path(key) + " 2>/dev/null")) {
        std::uint64_t n = 0;
        std::size_t start = 0;
        while (start < out->size()) {
          auto end = out->find('\n', start);
          if (end == std::string::npos) {
            end = out->size();
          }
          if (end > start) {
            std::string_view line{out->data() + start, end - start};
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
              line.remove_suffix(1);
            }
            if (!line.empty() && line.back() != '/') {
              ++n;
            }
          }
          start = end + 1;
        }
        info.file_count = n;
      }
    }
  }
}

} // namespace

void clear_media_probe_cache()
{
  std::lock_guard lock(g_cache_mutex);
  g_cache.clear();
}

bool is_archive_or_document_name(const std::string& name)
{
  // Types we never send to ffprobe (it only emits "Invalid data…" noise).
  static constexpr const char* kSkip[] = {
      ".zip",  ".tar",  ".tgz", ".7z",  ".rar",  ".cbz", ".cbr", ".jar", ".apk",
      ".tar.gz", ".tar.bz2", ".tar.xz", ".gz", ".bz2", ".xz", ".pdf",
      ".iso", ".img", ".dmg", ".exe", ".dll", ".so", ".a", ".o", ".class",
      ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx", ".odt", ".ods", ".odp",
      ".txt", ".md", ".json", ".xml", ".html", ".htm", ".css", ".js", ".ts",
      ".py", ".cpp", ".hpp", ".c", ".h", ".rs", ".go", ".java", ".kt",
  };
  for (const char* s : kSkip) {
    if (path_ends_with_ci(name, s)) {
      return true;
    }
  }
  return false;
}

std::optional<MediaInfo> probe_media_raw(const std::filesystem::path& path)
{
  const std::string key = path.string();
  {
    std::lock_guard lock(g_cache_mutex);
    if (const auto it = g_cache.find(key); it != g_cache.end()) {
      return it->second;
    }
  }

  const std::string name = path.filename().string();
  std::optional<MediaInfo> result;

  // Skip ffprobe for archives/PDFs/docs — they only produce FFmpeg stderr noise
  // ("Invalid data found when processing input"). File counts / PDF pages come
  // from enrich_pages_and_filecount (bsdtar / pdfinfo) instead.
  if (!is_archive_or_document_name(name)) {
    const std::string ffprobe = find_ffprobe();
    // Redirect stderr so unknown/non-media files cannot spam the console.
    std::string cmd = ffprobe + " -v error -show_entries "
                      "stream=width,height,r_frame_rate,avg_frame_rate:format=duration "
                      "-of default=noprint_wrappers=1:nokey=0 ";
    cmd += quote_path(key);
    cmd += " 2>/dev/null";

    if (auto out = run_capture(cmd)) {
      auto info = parse_ffprobe_output(*out);
      if (info.width || info.height || info.duration_ms || info.framerate) {
        result = info;
      }
    }
  }

  if (result) {
    enrich_pages_and_filecount(path, *result);
  } else {
    MediaInfo only_meta;
    enrich_pages_and_filecount(path, only_meta);
    if (only_meta.pages || only_meta.file_count) {
      result = only_meta;
    }
  }

  {
    std::lock_guard lock(g_cache_mutex);
    g_cache[key] = result;
  }
  return result;
}

std::optional<MediaInfo> probe_media(const std::filesystem::path& path)
{
  return probe_media_raw(path);
}

} // namespace dirtoo::filter

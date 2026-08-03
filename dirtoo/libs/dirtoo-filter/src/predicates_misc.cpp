// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/predicates.hpp"
#include "predicates_detail.hpp"

#include <algorithm>
#include <fstream>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cctype>
#include <optional>
#include <vector>
#include <string>
#include <string_view>
#include <regex>
#include <filesystem>
#include <system_error>
#include <random>
#include <cstdlib>

namespace dirtoo::filter {
using detail::LenCmp;
using detail::split_len_cmp;
using detail::apply_len_cmp;
using detail::lower_copy;
using detail::resolve_mtime_sec;
using detail::lookup_media;

namespace {

class RandomMatch : public MatchFunc {
public:
  explicit RandomMatch(double probability)
      : probability_(std::clamp(probability, 0.0, 1.0))
      , eng_(std::random_device{}())
  {
  }
  bool matches(const FilterItem&) const override
  {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(eng_) < probability_;
  }

private:
  double probability_;
  mutable std::mt19937 eng_;
};

class CharsetMatch : public MatchFunc {
public:
  explicit CharsetMatch(std::string charset)
      : charset_(std::move(charset))
  {
  }
  bool matches(const FilterItem& item) const override
  {
    // Practical subset without iconv: ascii / utf-8 / latin1 / iso-8859-1
    const auto lower = lower_copy(charset_);
    if (lower == "ascii" || lower == "us-ascii") {
      for (unsigned char c : item.name) {
        if (c > 127) {
          return false;
        }
      }
      return true;
    }
    if (lower == "latin1" || lower == "latin-1" || lower == "iso-8859-1" || lower == "iso8859-1") {
      // All byte sequences are valid latin1; accept any basename
      return true;
    }
    if (lower == "utf-8" || lower == "utf8") {
      // Validate UTF-8 of basename
      const auto& s = item.name;
      std::size_t i = 0;
      while (i < s.size()) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
          ++i;
          continue;
        }
        int need = 0;
        if ((c & 0xE0) == 0xC0) {
          need = 1;
        } else if ((c & 0xF0) == 0xE0) {
          need = 2;
        } else if ((c & 0xF8) == 0xF0) {
          need = 3;
        } else {
          return false;
        }
        if (i + static_cast<std::size_t>(need) >= s.size()) {
          return false;
        }
        for (int k = 1; k <= need; ++k) {
          if ((static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]) & 0xC0) != 0x80) {
            return false;
          }
        }
        i += static_cast<std::size_t>(need) + 1;
      }
      return true;
    }
    // Unknown charset name → never match (visible failure)
    return false;
  }

private:
  std::string charset_;
};


} // namespace

MatchFuncPtr make_random(std::string argument)
{
  try {
    const double p = std::stod(argument);
    return std::make_shared<RandomMatch>(p);
  } catch (...) {
    return std::make_shared<AlwaysFalse>();
  }
}

MatchFuncPtr make_charset(std::string argument)
{
  while (!argument.empty() && std::isspace(static_cast<unsigned char>(argument.front()))) {
    argument.erase(argument.begin());
  }
  if (argument.empty()) {
    return std::make_shared<AlwaysFalse>();
  }
  return std::make_shared<CharsetMatch>(std::move(argument));
}


namespace {

std::mutex g_pages_mu;
std::map<std::string, std::optional<std::uint64_t>> g_pages_cache;
std::mutex g_fc_mu;
std::map<std::string, std::optional<std::uint64_t>> g_filecount_cache;

std::string shell_quote(const std::string& s)
{
  std::string out = "'";
  for (char c : s) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out += c;
    }
  }
  out += "'";
  return out;
}

std::optional<std::string> run_capture_cmd(const std::string& cmd)
{
  FILE* pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr) {
    return std::nullopt;
  }
  std::string out;
  char buf[512];
  while (fgets(buf, sizeof(buf), pipe) != nullptr) {
    out += buf;
  }
  const int rc = pclose(pipe);
  if (rc != 0 && out.empty()) {
    return std::nullopt;
  }
  return out;
}

std::optional<std::uint64_t> probe_pdf_pages(const std::filesystem::path& path)
{
  const std::string key = path.string();
  {
    std::lock_guard lock(g_pages_mu);
    if (const auto it = g_pages_cache.find(key); it != g_pages_cache.end()) {
      return it->second;
    }
  }
  std::optional<std::uint64_t> result;
  // Prefer pdfinfo (poppler)
  const char* pdfinfo = std::getenv("DIRTOO_PDFINFO");
  const std::string tool = (pdfinfo && pdfinfo[0]) ? pdfinfo : "pdfinfo";
  if (auto out = run_capture_cmd(tool + " " + shell_quote(key) + " 2>/dev/null")) {
    // Pages: 12
    for (std::size_t i = 0; i + 6 < out->size(); ++i) {
      if ((*out)[i] == 'P' && out->compare(i, 6, "Pages:") == 0) {
        std::size_t j = i + 6;
        while (j < out->size() && std::isspace(static_cast<unsigned char>((*out)[j]))) {
          ++j;
        }
        try {
          result = static_cast<std::uint64_t>(std::stoull(out->substr(j)));
        } catch (...) {
        }
        break;
      }
    }
  }
  if (!result) {
    // Bounded PDF text scan: count "/Type /Page" not "/Type /Pages"
    std::ifstream in(path, std::ios::binary);
    if (in) {
      constexpr std::size_t kMax = 8u << 20; // 8 MiB
      std::string data(kMax, '\0');
      in.read(data.data(), static_cast<std::streamsize>(kMax));
      data.resize(static_cast<std::size_t>(in.gcount()));
      std::uint64_t count = 0;
      const std::string needle = "/Type";
      std::size_t pos = 0;
      while ((pos = data.find(needle, pos)) != std::string::npos) {
        pos += needle.size();
        while (pos < data.size() && std::isspace(static_cast<unsigned char>(data[pos]))) {
          ++pos;
        }
        if (pos + 5 <= data.size() && data.compare(pos, 5, "/Page") == 0) {
          // Not /Pages
          if (pos + 5 >= data.size() || data[pos + 5] != 's') {
            ++count;
          }
        }
      }
      if (count > 0) {
        result = count;
      }
    }
  }
  {
    std::lock_guard lock(g_pages_mu);
    g_pages_cache[key] = result;
  }
  return result;
}

bool looks_like_archive(const std::filesystem::path& path)
{
  std::string ext = path.extension().string();
  for (char& c : ext) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  static const char* kExts[] = {".zip", ".tar", ".tgz", ".gz", ".bz2", ".xz", ".7z", ".rar",
                                ".cbz", ".cbr", ".iso", ".jar", ".apk"};
  for (const char* e : kExts) {
    if (ext == e) {
      return true;
    }
  }
  // .tar.gz etc.
  const auto name = path.filename().string();
  std::string lower = name;
  for (char& c : lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return lower.ends_with(".tar.gz") || lower.ends_with(".tar.bz2") || lower.ends_with(".tar.xz");
}

std::optional<std::uint64_t> probe_archive_filecount(const std::filesystem::path& path)
{
  const std::string key = path.string();
  {
    std::lock_guard lock(g_fc_mu);
    if (const auto it = g_filecount_cache.find(key); it != g_filecount_cache.end()) {
      return it->second;
    }
  }
  std::optional<std::uint64_t> result;
  const std::string q = shell_quote(key);
  // Prefer bsdtar -tf (files only roughly: count lines that are not trailing /)
  auto count_lines = [](const std::string& text) -> std::uint64_t {
    std::uint64_t n = 0;
    std::size_t start = 0;
    while (start < text.size()) {
      auto end = text.find('\n', start);
      if (end == std::string::npos) {
        end = text.size();
      }
      if (end > start) {
        std::string_view line{text.data() + start, end - start};
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
          line.remove_suffix(1);
        }
        if (!line.empty() && line.back() != '/') {
          ++n;
        }
      }
      start = end + 1;
    }
    return n;
  };
  if (auto out = run_capture_cmd("bsdtar -tf " + q + " 2>/dev/null")) {
    result = count_lines(*out);
  } else if (auto out = run_capture_cmd("unzip -Z1 " + q + " 2>/dev/null")) {
    result = count_lines(*out);
  } else if (auto out = run_capture_cmd("7z l -ba " + q + " 2>/dev/null")) {
    // 7z -ba lines are less structured; count non-empty
    result = count_lines(*out);
  }
  {
    std::lock_guard lock(g_fc_mu);
    g_filecount_cache[key] = result;
  }
  return result;
}

class PagesMatch : public MatchFunc {
public:
  PagesMatch(LenCmp op, double value)
      : op_(op)
      , value_(value)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    if (item.is_directory || item.path.empty()) {
      return false;
    }
    if (const auto meta = detail::lookup_media(item.path); meta && meta->pages) {
      return apply_len_cmp(op_, static_cast<double>(*meta->pages), value_);
    }
    const auto pages = probe_pdf_pages(item.path);
    if (!pages) {
      return false;
    }
    return apply_len_cmp(op_, static_cast<double>(*pages), value_);
  }

private:
  LenCmp op_;
  double value_;
};

class FileCountMatch : public MatchFunc {
public:
  FileCountMatch(LenCmp op, double value)
      : op_(op)
      , value_(value)
  {
  }
  bool matches(const FilterItem& item) const override
  {
    if (item.is_directory || item.path.empty()) {
      return false;
    }
    if (const auto meta = detail::lookup_media(item.path); meta && meta->file_count) {
      return apply_len_cmp(op_, static_cast<double>(*meta->file_count), value_);
    }
    if (!looks_like_archive(item.path)) {
      return false;
    }
    const auto n = probe_archive_filecount(item.path);
    if (!n) {
      return false;
    }
    return apply_len_cmp(op_, static_cast<double>(*n), value_);
  }

private:
  LenCmp op_;
  double value_;
};

} // namespace

MatchFuncPtr make_pages(std::string argument)
{
  const auto [op, rest] = split_len_cmp(argument);
  auto trimmed = rest;
  while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
    trimmed.remove_prefix(1);
  }
  if (trimmed.empty()) {
    return std::make_shared<AlwaysFalse>();
  }
  try {
    const double v = std::stod(std::string{trimmed});
    return std::make_shared<PagesMatch>(op, v);
  } catch (...) {
    return std::make_shared<AlwaysFalse>();
  }
}

MatchFuncPtr make_filecount(std::string argument)
{
  const auto [op, rest] = split_len_cmp(argument);
  auto trimmed = rest;
  while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
    trimmed.remove_prefix(1);
  }
  if (trimmed.empty()) {
    return std::make_shared<AlwaysFalse>();
  }
  try {
    const double v = std::stod(std::string{trimmed});
    return std::make_shared<FileCountMatch>(op, v);
  } catch (...) {
    return std::make_shared<AlwaysFalse>();
  }
}



} // namespace dirtoo::filter

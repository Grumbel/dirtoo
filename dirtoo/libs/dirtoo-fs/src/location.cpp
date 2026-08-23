// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/fs/location.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace dirtoo::fs {
namespace {

std::filesystem::path normalize_file_path(std::filesystem::path path)
{
  if (!path.is_absolute()) {
    std::error_code ec;
    path = std::filesystem::absolute(path, ec);
    if (ec) {
      // Keep best-effort path; callers still navigate with the string they typed.
      return path.lexically_normal();
    }
  }
  // Do not call weakly_canonical: it follows the filesystem and can block
  // indefinitely on hung NFS/SMB mounts (GUI navigate path). Lexical cleanup
  // is enough for Location keys; listing uses the path as given.
  return path.lexically_normal();
}

bool is_hex_digit(char c)
{
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int hex_value(char c)
{
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

/// Encode a filesystem path for use inside a file:// URL.
/// Keeps '/' separators and unreserved ASCII; percent-encodes spaces, '#', '?',
/// '%', control chars, non-ASCII bytes, and other reserved delimiters.
std::string percent_encode_path(const std::string& path)
{
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(path.size() + 8);
  for (unsigned char uc : path) {
    const char c = static_cast<char>(uc);
    const bool unreserved =
        (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
        || c == '-' || c == '.' || c == '_' || c == '~' || c == '/';
    if (unreserved) {
      out += c;
    } else {
      out += '%';
      out += kHex[uc >> 4];
      out += kHex[uc & 0x0F];
    }
  }
  return out;
}

std::string percent_decode(std::string_view in)
{
  std::string out;
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size(); ++i) {
    if (in[i] == '%' && i + 2 < in.size() && is_hex_digit(in[i + 1]) && is_hex_digit(in[i + 2])) {
      const int hi = hex_value(in[i + 1]);
      const int lo = hex_value(in[i + 2]);
      out += static_cast<char>((hi << 4) | lo);
      i += 2;
      continue;
    }
    out += in[i];
  }
  return out;
}

/// Encode a query parameter value (?filter= / ?search=).
std::string percent_encode_query(const std::string& value)
{
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(value.size() + 8);
  for (unsigned char uc : value) {
    const char c = static_cast<char>(uc);
    const bool unreserved =
        (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
        || c == '-' || c == '.' || c == '_' || c == '~';
    if (unreserved) {
      out += c;
    } else {
      out += '%';
      out += kHex[uc >> 4];
      out += kHex[uc & 0x0F];
    }
  }
  return out;
}

/// Split optional ?filter=&search= suffix. Returns path-without-query.
std::string strip_and_parse_query(std::string_view url, std::string* filter, std::string* search)
{
  if (filter != nullptr) {
    filter->clear();
  }
  if (search != nullptr) {
    search->clear();
  }
  // Query starts at the last '?' so archive "file:///x.zip//archive:a?b" still works
  // only when intentionally used as query (rare). Prefer first '?' after scheme body.
  const auto qpos = url.find('?');
  if (qpos == std::string_view::npos) {
    return std::string{url};
  }
  const std::string_view base = url.substr(0, qpos);
  const std::string_view query = url.substr(qpos + 1);
  auto take_param = [&](std::string_view key, std::string* dest) {
    if (dest == nullptr) {
      return;
    }
    std::size_t start = 0;
    while (start < query.size()) {
      auto amp = query.find('&', start);
      if (amp == std::string_view::npos) {
        amp = query.size();
      }
      const auto part = query.substr(start, amp - start);
      const auto eq = part.find('=');
      const auto k = eq == std::string_view::npos ? part : part.substr(0, eq);
      const auto v = eq == std::string_view::npos ? std::string_view{} : part.substr(eq + 1);
      if (k == key) {
        *dest = percent_decode(v);
        return;
      }
      start = amp + 1;
    }
  };
  take_param("filter", filter);
  take_param("search", search);
  return std::string{base};
}

std::string append_query(std::string url, const std::string& filter, const std::string& search)
{
  if (filter.empty() && search.empty()) {
    return url;
  }
  url += '?';
  bool first = true;
  if (!filter.empty()) {
    url += "filter=";
    url += percent_encode_query(filter);
    first = false;
  }
  if (!search.empty()) {
    if (!first) {
      url += '&';
    }
    url += "search=";
    url += percent_encode_query(search);
  }
  return url;
}

} // namespace

bool looks_like_archive(const std::filesystem::path& path)
{
  auto ext = path.extension().string();
  if (ext.empty()) {
    return false;
  }
  if (ext[0] == '.') {
    ext.erase(ext.begin());
  }
  for (char& c : ext) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  static constexpr std::string_view kExts[] = {
      "zip", "tar", "gz", "tgz", "bz2", "xz", "7z", "rar", "jar", "apk", "cbz", "cbr",
  };
  // Also handle .tar.gz etc. via stem.
  const auto name = path.filename().string();
  auto lower = name;
  for (char& c : lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (lower.ends_with(".tar.gz") || lower.ends_with(".tar.bz2") || lower.ends_with(".tar.xz")
      || lower.ends_with(".tar.zst")) {
    return true;
  }
  return std::ranges::find(kExts, std::string_view{ext}) != std::end(kExts);
}

Location::Location(std::string protocol, std::filesystem::path path, std::filesystem::path entry)
    : protocol_(std::move(protocol))
    , path_(std::move(path))
    , entry_(std::move(entry))
{
}

Location Location::from_path(const std::filesystem::path& path)
{
  return Location{"file", normalize_file_path(path), {}};
}

Location Location::from_path_unchecked(std::filesystem::path path)
{
  return Location{"file", std::move(path), {}};
}

Location Location::from_archive(const std::filesystem::path& archive_file,
                                const std::filesystem::path& entry)
{
  return Location{"archive", normalize_file_path(archive_file), entry.lexically_normal()};
}

Location Location::from_tag(std::string_view tag_name)
{
  std::string name{tag_name};
  while (!name.empty() && (name.front() == ' ' || name.front() == '	')) {
    name.erase(name.begin());
  }
  while (!name.empty() && (name.back() == ' ' || name.back() == '	')) {
    name.pop_back();
  }
  return Location{"tag", std::filesystem::path{name}, {}};
}

Location Location::from_set(std::string_view set_id_or_label)
{
  std::string name{set_id_or_label};
  while (!name.empty() && (name.front() == ' ' || name.front() == '	')) {
    name.erase(name.begin());
  }
  while (!name.empty() && (name.back() == ' ' || name.back() == '	')) {
    name.pop_back();
  }
  return Location{"set", std::filesystem::path{name}, {}};
}

Location Location::from_url(std::string_view url)
{
  std::string filter;
  std::string search;
  const std::string stripped = strip_and_parse_query(url, &filter, &search);
  url = stripped;

  Location loc;
  if (url.starts_with("tag://")) {
    loc = from_tag(percent_decode(std::string{url.substr(6)}));
  } else if (url.starts_with("set://")) {
    loc = from_set(percent_decode(std::string{url.substr(6)}));
  } else if (url.starts_with("file://")) {
    std::string rest{url.substr(7)};
    const auto payload_sep = rest.find("//");
    if (payload_sep != std::string::npos) {
      const std::string abspath = percent_decode(rest.substr(0, payload_sep));
      std::string payload = rest.substr(payload_sep + 2);
      const auto next = payload.find("//");
      if (next != std::string::npos) {
        payload = payload.substr(0, next);
      }
      const auto colon = payload.find(':');
      const std::string kind = (colon == std::string::npos) ? payload : payload.substr(0, colon);
      const std::string entry =
          (colon == std::string::npos) ? std::string{} : percent_decode(payload.substr(colon + 1));
      if (kind == "archive") {
        loc = from_archive(std::filesystem::path{abspath}, std::filesystem::path{entry});
      } else {
        loc = from_path(std::filesystem::path{abspath});
      }
    } else {
      loc = from_path(std::filesystem::path{percent_decode(rest)});
    }
  } else if (url.starts_with("archive://")) {
    std::string rest{percent_decode(url.substr(10))};
    const auto bang = rest.find("!/");
    if (bang == std::string::npos) {
      loc = from_archive(std::filesystem::path{rest}, {});
    } else {
      loc = from_archive(std::filesystem::path{rest.substr(0, bang)},
                         std::filesystem::path{rest.substr(bang + 2)});
    }
  } else {
    throw std::invalid_argument("unsupported URL scheme");
  }

  if (!filter.empty() || !search.empty()) {
    loc = loc.with_filter_and_search(filter, search);
  }
  return loc;
}

Location Location::from_human(std::string_view text)
{
  if (text.empty() || text == ".") {
    return from_path(std::filesystem::current_path());
  }
  if (text.starts_with("file://") || text.starts_with("archive://")
      || text.starts_with("tag://") || text.starts_with("set://")) {
    return from_url(text);
  }
  // Bare path that embeds Python-style //archive payload (typed without scheme).
  if (text.find("//archive") != std::string_view::npos) {
    return from_url(std::string("file://") + std::string{text});
  }
  // Bare path may still carry ?filter=&search= (typed in the location bar).
  if (text.find('?') != std::string_view::npos) {
    return from_url(std::string("file://") + std::string{text});
  }
  return from_path(std::filesystem::path{std::string{text}});
}

std::string Location::as_url() const
{
  std::string out;
  if (protocol_ == "tag") {
    out = "tag://" + percent_encode_path(path_.generic_string());
  } else if (protocol_ == "set") {
    out = "set://" + percent_encode_path(path_.generic_string());
  } else if (protocol_ == "archive") {
    // Prefer Python-style URLs so the location bar matches dirtoo-py:
    //   file:///path/to.zip//archive
    //   file:///path/to.zip//archive:docs/readme.txt
    out = "file://";
    out += percent_encode_path(path_.string());
    out += "//archive";
    if (!entry_.empty()) {
      out += ':';
      out += percent_encode_path(entry_.generic_string());
    }
  } else {
    out = "file://" + percent_encode_path(path_.string());
  }
  return append_query(std::move(out), filter_, search_);
}

Location Location::with_filter(std::string_view filter) const
{
  Location loc = *this;
  loc.filter_ = std::string{filter};
  return loc;
}

Location Location::with_search(std::string_view search) const
{
  Location loc = *this;
  loc.search_ = std::string{search};
  return loc;
}

Location Location::with_filter_and_search(std::string_view filter, std::string_view search) const
{
  Location loc = *this;
  loc.filter_ = std::string{filter};
  loc.search_ = std::string{search};
  return loc;
}

std::filesystem::path Location::as_path() const
{
  if (protocol_ == "tag" || protocol_ == "set") {
    return {};
  }
  return path_;
}

std::string Location::tag_query() const
{
  if (protocol_ != "tag") {
    return {};
  }
  return path_.generic_string();
}

std::string Location::set_query() const
{
  if (protocol_ != "set") {
    return {};
  }
  return path_.generic_string();
}

Location Location::parent() const
{
  if (protocol_ == "tag" || protocol_ == "set") {
    return {};
  }
  Location result;
  if (protocol_ == "archive") {
    if (entry_.empty() || entry_ == "." || entry_ == "/") {
      // Leave archive → parent directory of the archive file.
      result = from_path(path_.parent_path());
    } else {
      auto parent_entry = entry_.parent_path();
      if (parent_entry == ".") {
        parent_entry.clear();
      }
      result = from_archive(path_, parent_entry);
    }
  } else if (path_.has_parent_path() && path_ != path_.root_path()) {
    result = from_path(path_.parent_path());
  } else {
    result = from_path(path_.root_path().empty() ? std::filesystem::path{"/"} : path_.root_path());
  }
  // Listing modifiers travel with the location so go-up / join keep the
  // active filter or search (same as pin-filter, encoded on the URL).
  result.filter_ = filter_;
  result.search_ = search_;
  return result;
}

Location Location::join(std::string_view child) const
{
  if (protocol_ == "tag" || protocol_ == "set") {
    (void)child;
    return *this;
  }
  Location result;
  if (protocol_ == "archive") {
    result = from_archive(path_, entry_ / std::filesystem::path{std::string{child}});
  } else {
    result = from_path(path_ / std::filesystem::path{std::string{child}});
  }
  result.filter_ = filter_;
  result.search_ = search_;
  return result;
}

std::string Location::basename() const
{
  if (protocol_ == "tag" || protocol_ == "set") {
    return path_.generic_string();
  }
  if (protocol_ == "archive") {
    if (!entry_.empty()) {
      return entry_.filename().string();
    }
    return path_.filename().string();
  }
  return path_.filename().string();
}

std::string Location::dirname() const
{
  if (protocol_ == "archive") {
    return entry_.parent_path().generic_string();
  }
  return path_.parent_path().string();
}

} // namespace dirtoo::fs

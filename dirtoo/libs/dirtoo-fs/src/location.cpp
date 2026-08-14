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
  if (url.starts_with("tag://")) {
    return from_tag(percent_decode(std::string{url.substr(6)}));
  }
  if (url.starts_with("set://")) {
    return from_set(percent_decode(std::string{url.substr(6)}));
  }
  // Python-style archive: file:///path/to.zip//archive or file:///path/to.zip//archive:entry
  // Legacy JAR-style:     archive:///path/to.zip!/entry
  if (url.starts_with("file://")) {
    std::string rest{url.substr(7)};
    // Split on "//" payload separators (Python Location payloads).
    const auto payload_sep = rest.find("//");
    if (payload_sep != std::string::npos) {
      const std::string abspath = percent_decode(rest.substr(0, payload_sep));
      std::string payload = rest.substr(payload_sep + 2); // e.g. "archive" or "archive:docs/a"
      // Nested payloads are rare; take the first.
      const auto next = payload.find("//");
      if (next != std::string::npos) {
        payload = payload.substr(0, next);
      }
      const auto colon = payload.find(':');
      std::string prot =
          colon == std::string::npos ? payload : payload.substr(0, colon);
      std::string entry =
          colon == std::string::npos ? std::string{} : percent_decode(payload.substr(colon + 1));
      for (char& c : prot) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }
      if (prot == "archive") {
        return from_archive(std::filesystem::path{abspath}, std::filesystem::path{entry});
      }
      // Unknown payload: treat as plain file path to the archive container.
      return from_path(std::filesystem::path{abspath});
    }
    return from_path(std::filesystem::path{percent_decode(rest)});
  }
  if (url.starts_with("archive://")) {
    // Backward-compatible JAR-inspired form: archive:///abs/file.zip!/inner
    std::string rest{percent_decode(url.substr(10))};
    const auto bang = rest.find("!/");
    if (bang == std::string::npos) {
      return from_archive(std::filesystem::path{rest}, {});
    }
    return from_archive(std::filesystem::path{rest.substr(0, bang)},
                        std::filesystem::path{rest.substr(bang + 2)});
  }
  throw std::invalid_argument("unsupported URL scheme");
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
  return from_path(std::filesystem::path{std::string{text}});
}

std::string Location::as_url() const
{
  if (protocol_ == "tag") {
    return "tag://" + percent_encode_path(path_.generic_string());
  }
  if (protocol_ == "set") {
    return "set://" + percent_encode_path(path_.generic_string());
  }
  // Prefer Python-style URLs so the location bar matches dirtoo-py:
  //   file:///path/to.zip//archive
  //   file:///path/to.zip//archive:docs/readme.txt
  if (protocol_ == "archive") {
    std::string out = "file://";
    out += percent_encode_path(path_.string());
    out += "//archive";
    if (!entry_.empty()) {
      out += ':';
      out += percent_encode_path(entry_.generic_string());
    }
    return out;
  }
  return "file://" + percent_encode_path(path_.string());
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
  if (protocol_ == "archive") {
    if (entry_.empty() || entry_ == "." || entry_ == "/") {
      // Leave archive → parent directory of the archive file.
      return from_path(path_.parent_path());
    }
    auto parent_entry = entry_.parent_path();
    if (parent_entry == ".") {
      parent_entry.clear();
    }
    return from_archive(path_, parent_entry);
  }

  if (path_.has_parent_path() && path_ != path_.root_path()) {
    return from_path(path_.parent_path());
  }
  return from_path(path_.root_path().empty() ? std::filesystem::path{"/"} : path_.root_path());
}

Location Location::join(std::string_view child) const
{
  if (protocol_ == "tag" || protocol_ == "set") {
    (void)child;
    return *this;
  }
  if (protocol_ == "archive") {
    return from_archive(path_, entry_ / std::filesystem::path{std::string{child}});
  }
  return from_path(path_ / std::filesystem::path{std::string{child}});
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

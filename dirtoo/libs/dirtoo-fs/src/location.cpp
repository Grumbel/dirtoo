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
    path = std::filesystem::absolute(path);
  }
  std::error_code ec;
  auto weak = std::filesystem::weakly_canonical(path, ec);
  if (!ec) {
    return weak;
  }
  return path;
}

std::string percent_encode_path(const std::string& path)
{
  std::string out;
  out.reserve(path.size());
  for (char c : path) {
    if (c == ' ') {
      out += "%20";
    } else {
      out += c;
    }
  }
  return out;
}

std::string percent_decode(std::string_view in)
{
  std::string out;
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size(); ++i) {
    if (in[i] == '%' && i + 2 < in.size()) {
      const auto hex = std::string{in.substr(i + 1, 2)};
      if (hex == "20") {
        out += ' ';
        i += 2;
        continue;
      }
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

Location Location::from_url(std::string_view url)
{
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
  if (text.starts_with("file://") || text.starts_with("archive://")) {
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
  return path_;
}

Location Location::parent() const
{
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
  if (protocol_ == "archive") {
    return from_archive(path_, entry_ / std::filesystem::path{std::string{child}});
  }
  return from_path(path_ / std::filesystem::path{std::string{child}});
}

std::string Location::basename() const
{
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

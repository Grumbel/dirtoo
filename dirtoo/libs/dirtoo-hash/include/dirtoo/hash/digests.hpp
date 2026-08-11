// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace dirtoo::hash {

/// Digests from a single sequential read of a regular file.
struct FileDigests {
  std::uint64_t size = 0;
  /// Nanoseconds since epoch when known (from stat).
  std::optional<std::int64_t> mtime_ns;

  std::string crc32_hex;  // 8 hex digits, lowercase
  std::string md5_hex;    // 32 hex
  std::string sha1_hex;   // 40 hex
  std::string sha256_hex; // 64 hex
};

} // namespace dirtoo::hash

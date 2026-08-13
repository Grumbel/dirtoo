// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/hash/digests.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace dirtoo::hash {

struct HashOptions {
  /// Cooperative cancel; polled between read chunks. Return true to abort.
  std::function<bool()> should_cancel;
  /// Optional progress: bytes_read, total_size (0 if unknown).
  std::function<void(std::uint64_t bytes_read, std::uint64_t total_size)> on_progress;
};

struct HashError {
  std::string message;
};

/// One sequential read → CRC32, MD5, SHA-1, SHA-256.
/// Regular files only; directories/symlinks-to-dirs fail.
[[nodiscard]] std::optional<FileDigests>
hash_file(const std::filesystem::path& path, const HashOptions& options = {},
          HashError* error = nullptr);

/// Sample-based digests: hash size + head / mid / tail windows.
/// Fast on multi-GB files; weaker than a full-file hash. Do not use as the
/// sole identity for tags (those still need hash_file / full SHA-256).
struct QuickHashOptions {
  /// Bytes per window (default 1 MiB). Clamped to at least 4 KiB.
  std::uint64_t window_bytes = 1ULL << 20;
  std::function<bool()> should_cancel;
};

/// Quick content fingerprint. Sets size, mtime_ns, sha256_hex (sample digest).
/// CRC32 is set to "quick" as a marker; MD5/SHA-1 stay empty.
/// Files ≤ 3·window use a normal full hash_file pass.
[[nodiscard]] std::optional<FileDigests>
hash_file_quick(const std::filesystem::path& path, const QuickHashOptions& options = {},
                HashError* error = nullptr);

} // namespace dirtoo::hash

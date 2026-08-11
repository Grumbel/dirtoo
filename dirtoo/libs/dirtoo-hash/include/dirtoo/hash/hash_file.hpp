// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/hash/digests.hpp"

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

} // namespace dirtoo::hash

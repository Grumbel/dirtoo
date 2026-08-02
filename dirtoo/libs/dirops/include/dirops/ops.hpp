// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirops/error.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <vector>

namespace dirops {

/// How to handle an existing destination path.
///
/// - Fail: return an error; leave both paths unchanged (default).
/// - Overwrite: remove the existing destination, then write/rename onto it.
/// - Rename: leave the existing destination alone; pick a free name via
///   unique_path(), e.g. "report.pdf" → "report (2).pdf".
/// - Skip: treat as success with ItemResult::skipped = true; source unchanged.
enum class ConflictPolicy {
  Fail,       ///< Return an error if the target exists
  Overwrite,  ///< Replace existing target
  Rename,     ///< Choose a non-conflicting name (stem (N).ext)
  Skip,       ///< Leave existing target unchanged
};

struct Options {
  bool dry_run = false;
  bool verbose = false;
  ConflictPolicy conflict = ConflictPolicy::Fail;

  /// Optional progress: (bytes_done, bytes_total, current_path). total may be 0 if unknown.
  std::function<void(std::uint64_t, std::uint64_t, const std::filesystem::path&)> on_progress;

  /// Return true to cancel the operation.
  std::function<bool()> is_cancelled;
};

struct ItemResult {
  std::filesystem::path source;
  std::filesystem::path destination;
  bool skipped = false;
};

struct Result {
  std::vector<ItemResult> items;
  bool cancelled = false;
};

using OpResult = std::expected<Result, Error>;

[[nodiscard]] OpResult copy_path(const std::filesystem::path& from,
                                 const std::filesystem::path& to,
                                 const Options& options = {});

[[nodiscard]] OpResult move_path(const std::filesystem::path& from,
                                 const std::filesystem::path& to,
                                 const Options& options = {});

[[nodiscard]] OpResult rename_path(const std::filesystem::path& from,
                                   const std::filesystem::path& to,
                                   const Options& options = {});

[[nodiscard]] OpResult remove_path(const std::filesystem::path& path,
                                   const Options& options = {});

[[nodiscard]] OpResult create_directory(const std::filesystem::path& path,
                                        const Options& options = {});

/// Create an empty regular file (fails if path already exists unless options say otherwise).
[[nodiscard]] OpResult create_file(const std::filesystem::path& path,
                                   const Options& options = {});

/// Create a symbolic link at link_path pointing at target.
[[nodiscard]] OpResult create_symlink(const std::filesystem::path& target,
                                      const std::filesystem::path& link_path,
                                      const Options& options = {});

/// Atomically swap two names on the same filesystem.
[[nodiscard]] OpResult swap_names(const std::filesystem::path& a,
                                  const std::filesystem::path& b,
                                  const Options& options = {});

} // namespace dirops

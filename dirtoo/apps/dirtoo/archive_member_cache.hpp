// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace dirtoo::app {

/// Cache root for extracted archive members used by drag-out and thumbnails.
[[nodiscard]] std::filesystem::path archive_member_cache_root(std::string_view subdir);

/// Deterministic extract directory for one archive file under @p cache_root.
[[nodiscard]] std::filesystem::path archive_member_dest_dir(
    const std::filesystem::path& cache_root,
    const std::filesystem::path& archive_file);

/// Return an existing extracted member path, or extract via libarchive.
/// Empty optional on failure.
[[nodiscard]] std::optional<std::filesystem::path>
ensure_archive_member_extracted(const std::filesystem::path& archive_file,
                                const std::filesystem::path& member,
                                const std::filesystem::path& dest_dir);

} // namespace dirtoo::app

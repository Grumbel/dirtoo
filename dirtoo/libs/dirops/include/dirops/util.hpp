// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>

namespace dirops {

/// Return a path in the same directory that does not exist yet.
/// e.g. "file.txt" -> "file (2).txt", "file (3).txt", ...
[[nodiscard]] std::filesystem::path unique_path(const std::filesystem::path& desired);

/// True if both paths are on the same filesystem (device).
[[nodiscard]] bool same_filesystem(const std::filesystem::path& a,
                                   const std::filesystem::path& b);

} // namespace dirops

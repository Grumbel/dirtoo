// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace dirtoo::app {

/// Replace @p path with @p content atomically: write a temporary file next to
/// it, flush, then rename over the target. A full disk, a crash or an error
/// part-way leaves the previous file untouched (a plain `ofstream(path,
/// trunc)` would have emptied it first). Creates parent directories.
/// On failure returns false and, if @p error is given, a message; the failure
/// is also logged.
[[nodiscard]] bool write_file_atomic(const std::filesystem::path& path,
                                     std::string_view content, std::string* error = nullptr);

} // namespace dirtoo::app

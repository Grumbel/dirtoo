// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace dirops {

/// Error type for filesystem operations. Carries errno-style code and path context.
class Error {
public:
  Error(std::error_code ec, std::filesystem::path path, std::string message = {})
      : ec_(ec)
      , path_(std::move(path))
      , message_(std::move(message))
  {
  }

  [[nodiscard]] std::error_code code() const noexcept { return ec_; }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
  [[nodiscard]] const std::string& message() const noexcept { return message_; }

  [[nodiscard]] std::string to_string() const;

private:
  std::error_code ec_;
  std::filesystem::path path_;
  std::string message_;
};

} // namespace dirops

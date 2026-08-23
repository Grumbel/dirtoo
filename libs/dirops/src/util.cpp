// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/util.hpp"

#include <format>
#include <system_error>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

namespace dirops {

std::filesystem::path unique_path(const std::filesystem::path& desired)
{
  namespace fs = std::filesystem;
  if (!fs::exists(desired)) {
    return desired;
  }

  const fs::path parent = desired.parent_path();
  const std::string stem = desired.stem().string();
  const std::string ext = desired.extension().string();

  for (int n = 2; n < 100000; ++n) {
    const auto candidate = parent / std::format("{} ({}){}", stem, n, ext);
    if (!fs::exists(candidate)) {
      return candidate;
    }
  }
  return desired;
}

bool same_filesystem(const std::filesystem::path& a, const std::filesystem::path& b)
{
#if defined(__linux__) || defined(__APPLE__)
  auto device_of = [](const std::filesystem::path& p) -> dev_t {
    struct ::stat st {};
    std::filesystem::path probe = p;
    std::error_code ec;
    if (!std::filesystem::exists(probe, ec)) {
      probe = p.parent_path();
      if (probe.empty()) {
        probe = ".";
      }
    }
    if (::stat(probe.c_str(), &st) != 0) {
      return static_cast<dev_t>(-1);
    }
    return st.st_dev;
  };
  const dev_t da = device_of(a);
  const dev_t db = device_of(b);
  if (da == static_cast<dev_t>(-1) || db == static_cast<dev_t>(-1)) {
    return false;
  }
  return da == db;
#else
  (void)a;
  (void)b;
  return true;
#endif
}

} // namespace dirops

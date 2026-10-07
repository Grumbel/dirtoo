// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/hash/file_stamp.hpp"

#include <chrono>

#include <sys/stat.h>

namespace dirtoo::hash {

std::optional<FileStamp> stat_stamp(const std::filesystem::path& path)
{
  struct stat st {};
  if (::stat(path.c_str(), &st) != 0) {
    return std::nullopt;
  }
  FileStamp s;
  s.size = static_cast<std::uint64_t>(st.st_size >= 0 ? st.st_size : 0);
  s.mtime_ns = static_cast<std::int64_t>(st.st_mtim.tv_sec) * 1'000'000'000LL +
               static_cast<std::int64_t>(st.st_mtim.tv_nsec);
  return s;
}

std::int64_t unix_ns_from_file_time(std::filesystem::file_time_type t)
{
  const auto sys = std::chrono::clock_cast<std::chrono::system_clock>(t);
  return std::chrono::duration_cast<std::chrono::nanoseconds>(sys.time_since_epoch()).count();
}

std::int64_t legacy_file_clock_offset_ns()
{
  // Legacy values were `count()` of file_time_type, i.e. ticks since the
  // file clock's own epoch. Unix ns = ticks (in ns) + (file epoch as Unix ns).
  const std::filesystem::file_time_type epoch{};
  const auto ticks_per_ns_num = std::filesystem::file_time_type::period::num;
  const auto ticks_per_ns_den = std::filesystem::file_time_type::period::den;
  if (ticks_per_ns_num != 1 || ticks_per_ns_den != 1'000'000'000) {
    return 0;  // not nanosecond ticks: cannot convert reliably
  }
  return unix_ns_from_file_time(epoch);
}

} // namespace dirtoo::hash

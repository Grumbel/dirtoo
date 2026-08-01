// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/ops.hpp"

#include <system_error>
#include <utility>

namespace dirops {
namespace {

bool cancelled(const Options& options)
{
  return options.is_cancelled && options.is_cancelled();
}

OpResult not_implemented(const std::filesystem::path& path, const char* what)
{
  return std::unexpected(Error{
      std::make_error_code(std::errc::operation_not_supported),
      path,
      std::string(what) + " is not implemented yet",
  });
}

} // namespace

OpResult copy_path(const std::filesystem::path& from,
                   const std::filesystem::path& to,
                   const Options& options)
{
  if (cancelled(options)) {
    return Result{.cancelled = true};
  }
  // TODO: implement recursive copy with conflict policy and progress.
  return not_implemented(from, "copy_path");
}

OpResult move_path(const std::filesystem::path& from,
                   const std::filesystem::path& to,
                   const Options& options)
{
  if (cancelled(options)) {
    return Result{.cancelled = true};
  }
  // TODO: rename when same device; otherwise copy+remove.
  return not_implemented(from, "move_path");
}

OpResult rename_path(const std::filesystem::path& from,
                     const std::filesystem::path& to,
                     const Options& options)
{
  if (cancelled(options)) {
    return Result{.cancelled = true};
  }

  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = from, .destination = to});
    return r;
  }

  std::error_code ec;
  std::filesystem::rename(from, to, ec);
  if (ec) {
    return std::unexpected(Error{ec, from, "rename failed"});
  }

  Result r;
  r.items.push_back(ItemResult{.source = from, .destination = to});
  return r;
}

OpResult remove_path(const std::filesystem::path& path, const Options& options)
{
  if (cancelled(options)) {
    return Result{.cancelled = true};
  }

  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = path, .destination = {}});
    return r;
  }

  std::error_code ec;
  const auto n = std::filesystem::remove_all(path, ec);
  if (ec) {
    return std::unexpected(Error{ec, path, "remove failed"});
  }
  (void)n;

  Result r;
  r.items.push_back(ItemResult{.source = path, .destination = {}});
  return r;
}

OpResult create_directory(const std::filesystem::path& path, const Options& options)
{
  if (cancelled(options)) {
    return Result{.cancelled = true};
  }

  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = {}, .destination = path});
    return r;
  }

  std::error_code ec;
  if (!std::filesystem::create_directory(path, ec) && ec) {
    return std::unexpected(Error{ec, path, "create_directory failed"});
  }

  Result r;
  r.items.push_back(ItemResult{.source = {}, .destination = path});
  return r;
}

OpResult swap_names(const std::filesystem::path& a,
                    const std::filesystem::path& b,
                    const Options& options)
{
  if (cancelled(options)) {
    return Result{.cancelled = true};
  }
  // TODO: temp-name swap on same device (like Python dt-swap).
  return not_implemented(a, "swap_names");
}

} // namespace dirops

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/ops.hpp"
#include "dirops/util.hpp"

#include <fstream>
#include <system_error>
#include <utility>

namespace dirops {
namespace {

bool cancelled(const Options& options)
{
  return options.is_cancelled && options.is_cancelled();
}

void report_progress(const Options& options,
                     std::uint64_t done,
                     std::uint64_t total,
                     const std::filesystem::path& path)
{
  if (options.on_progress) {
    options.on_progress(done, total, path);
  }
}

/// Resolve destination path according to conflict policy.
/// Returns nullopt destination with skipped=true when Skip applies.
struct ResolvedDest {
  std::filesystem::path path;
  bool skipped = false;
};

std::expected<ResolvedDest, Error> resolve_destination(const std::filesystem::path& to,
                                                       const Options& options)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::exists(to, ec)) {
    return ResolvedDest{.path = to};
  }

  switch (options.conflict) {
  case ConflictPolicy::Fail:
    return std::unexpected(Error{
        std::make_error_code(std::errc::file_exists),
        to,
        "destination already exists",
    });
  case ConflictPolicy::Overwrite:
    return ResolvedDest{.path = to};
  case ConflictPolicy::Rename:
    return ResolvedDest{.path = unique_path(to)};
  case ConflictPolicy::Skip:
    return ResolvedDest{.path = to, .skipped = true};
  }
  return ResolvedDest{.path = to};
}

OpResult copy_regular_file(const std::filesystem::path& from,
                           const std::filesystem::path& to,
                           const Options& options)
{
  namespace fs = std::filesystem;

  auto resolved = resolve_destination(to, options);
  if (!resolved) {
    return std::unexpected(resolved.error());
  }
  if (resolved->skipped) {
    Result r;
    r.items.push_back(ItemResult{.source = from, .destination = to, .skipped = true});
    return r;
  }

  const auto dest = resolved->path;

  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = from, .destination = dest});
    return r;
  }

  if (options.conflict == ConflictPolicy::Overwrite && fs::exists(dest)) {
    std::error_code ec;
    fs::remove(dest, ec);
    if (ec) {
      return std::unexpected(Error{ec, dest, "failed to remove existing destination"});
    }
  }

  std::error_code ec;
  fs::copy_file(from, dest, fs::copy_options::none, ec);
  if (ec) {
    return std::unexpected(Error{ec, from, "copy_file failed"});
  }

  std::uint64_t size = 0;
  {
    std::error_code sec;
    size = static_cast<std::uint64_t>(fs::file_size(dest, sec));
  }
  report_progress(options, size, size, dest);

  Result r;
  r.items.push_back(ItemResult{.source = from, .destination = dest});
  return r;
}

OpResult copy_directory_recursive(const std::filesystem::path& from,
                                  const std::filesystem::path& to,
                                  const Options& options,
                                  Result& acc)
{
  namespace fs = std::filesystem;

  if (cancelled(options)) {
    acc.cancelled = true;
    return acc;
  }

  auto resolved = resolve_destination(to, options);
  if (!resolved) {
    return std::unexpected(resolved.error());
  }
  if (resolved->skipped) {
    acc.items.push_back(ItemResult{.source = from, .destination = to, .skipped = true});
    return acc;
  }

  const auto dest_root = resolved->path;

  if (!options.dry_run) {
    std::error_code ec;
    fs::create_directories(dest_root, ec);
    if (ec && !fs::is_directory(dest_root)) {
      return std::unexpected(Error{ec, dest_root, "create_directories failed"});
    }
  }

  acc.items.push_back(ItemResult{.source = from, .destination = dest_root});

  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(from, ec)) {
    if (ec) {
      return std::unexpected(Error{ec, from, "directory iteration failed"});
    }
    if (cancelled(options)) {
      acc.cancelled = true;
      return acc;
    }

    const auto& src = entry.path();
    const auto dst = dest_root / src.filename();

    if (entry.is_symlink()) {
      // Copy symlink as symlink when possible.
      if (!options.dry_run) {
        std::error_code lec;
        const auto target = fs::read_symlink(src, lec);
        if (lec) {
          return std::unexpected(Error{lec, src, "read_symlink failed"});
        }
        if (options.conflict == ConflictPolicy::Overwrite && fs::exists(dst)) {
          fs::remove(dst, lec);
        }
        fs::create_symlink(target, dst, lec);
        if (lec) {
          return std::unexpected(Error{lec, dst, "create_symlink failed"});
        }
      }
      acc.items.push_back(ItemResult{.source = src, .destination = dst});
    } else if (entry.is_directory()) {
      auto sub = copy_directory_recursive(src, dst, options, acc);
      if (!sub) {
        return sub;
      }
      if (sub->cancelled) {
        return sub;
      }
    } else if (entry.is_regular_file()) {
      auto file_result = copy_regular_file(src, dst, options);
      if (!file_result) {
        return file_result;
      }
      for (auto& item : file_result->items) {
        acc.items.push_back(std::move(item));
      }
    }
  }
  return acc;
}

} // namespace

OpResult copy_path(const std::filesystem::path& from,
                   const std::filesystem::path& to,
                   const Options& options)
{
  namespace fs = std::filesystem;

  if (cancelled(options)) {
    return Result{.items = {}, .cancelled = true};
  }

  std::error_code ec;
  if (!fs::exists(from, ec)) {
    return std::unexpected(Error{
        ec ? ec : std::make_error_code(std::errc::no_such_file_or_directory),
        from,
        "source does not exist",
    });
  }

  // If destination is an existing directory, place source basename inside it.
  fs::path dest = to;
  if (fs::is_directory(to, ec) && !ec) {
    dest = to / from.filename();
  }

  if (fs::is_directory(from, ec)) {
    Result acc;
    return copy_directory_recursive(from, dest, options, acc);
  }
  if (fs::is_symlink(from, ec)) {
    auto resolved = resolve_destination(dest, options);
    if (!resolved) {
      return std::unexpected(resolved.error());
    }
    if (resolved->skipped) {
      Result r;
      r.items.push_back(ItemResult{.source = from, .destination = dest, .skipped = true});
      return r;
    }
    if (!options.dry_run) {
      std::error_code lec;
      const auto target = fs::read_symlink(from, lec);
      if (lec) {
        return std::unexpected(Error{lec, from, "read_symlink failed"});
      }
      if (options.conflict == ConflictPolicy::Overwrite && fs::exists(resolved->path)) {
        fs::remove(resolved->path, lec);
      }
      fs::create_symlink(target, resolved->path, lec);
      if (lec) {
        return std::unexpected(Error{lec, resolved->path, "create_symlink failed"});
      }
    }
    Result r;
    r.items.push_back(ItemResult{.source = from, .destination = resolved->path});
    return r;
  }

  return copy_regular_file(from, dest, options);
}

OpResult move_path(const std::filesystem::path& from,
                   const std::filesystem::path& to,
                   const Options& options)
{
  namespace fs = std::filesystem;

  if (cancelled(options)) {
    return Result{.items = {}, .cancelled = true};
  }

  std::error_code ec;
  if (!fs::exists(from, ec)) {
    return std::unexpected(Error{
        ec ? ec : std::make_error_code(std::errc::no_such_file_or_directory),
        from,
        "source does not exist",
    });
  }

  fs::path dest = to;
  if (fs::is_directory(to, ec) && !ec) {
    dest = to / from.filename();
  }

  auto resolved = resolve_destination(dest, options);
  if (!resolved) {
    return std::unexpected(resolved.error());
  }
  if (resolved->skipped) {
    Result r;
    r.items.push_back(ItemResult{.source = from, .destination = dest, .skipped = true});
    return r;
  }
  dest = resolved->path;

  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = from, .destination = dest});
    return r;
  }

  // Prefer atomic rename when on the same filesystem.
  if (same_filesystem(from, dest)) {
    if (options.conflict == ConflictPolicy::Overwrite && fs::exists(dest)) {
      fs::remove_all(dest, ec);
      if (ec) {
        return std::unexpected(Error{ec, dest, "failed to remove existing destination"});
      }
    }
    fs::rename(from, dest, ec);
    if (!ec) {
      Result r;
      r.items.push_back(ItemResult{.source = from, .destination = dest});
      return r;
    }
    // Fall through on cross-device style failures.
    if (ec != std::errc::cross_device_link) {
      return std::unexpected(Error{ec, from, "rename failed"});
    }
  }

  // Cross-device: copy then remove source.
  Options copy_opts = options;
  copy_opts.conflict = ConflictPolicy::Overwrite;
  auto copied = copy_path(from, dest, copy_opts);
  if (!copied) {
    return copied;
  }

  fs::remove_all(from, ec);
  if (ec) {
    return std::unexpected(Error{ec, from, "copied but failed to remove source"});
  }

  // Rewrite item sources for clarity.
  for (auto& item : copied->items) {
    if (item.source == from) {
      item.source = from;
    }
  }
  return copied;
}

OpResult rename_path(const std::filesystem::path& from,
                     const std::filesystem::path& to,
                     const Options& options)
{
  if (cancelled(options)) {
    return Result{.items = {}, .cancelled = true};
  }

  auto resolved = resolve_destination(to, options);
  if (!resolved) {
    return std::unexpected(resolved.error());
  }
  if (resolved->skipped) {
    Result r;
    r.items.push_back(ItemResult{.source = from, .destination = to, .skipped = true});
    return r;
  }

  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = from, .destination = resolved->path});
    return r;
  }

  std::error_code ec;
  if (options.conflict == ConflictPolicy::Overwrite
      && std::filesystem::exists(resolved->path)
      && resolved->path != from) {
    std::filesystem::remove_all(resolved->path, ec);
    if (ec) {
      return std::unexpected(Error{ec, resolved->path, "failed to remove existing destination"});
    }
  }

  std::filesystem::rename(from, resolved->path, ec);
  if (ec) {
    return std::unexpected(Error{ec, from, "rename failed"});
  }

  Result r;
  r.items.push_back(ItemResult{.source = from, .destination = resolved->path});
  return r;
}

OpResult remove_path(const std::filesystem::path& path, const Options& options)
{
  if (cancelled(options)) {
    return Result{.items = {}, .cancelled = true};
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
  if (n == 0 && !std::filesystem::exists(path)) {
    return std::unexpected(Error{
        std::make_error_code(std::errc::no_such_file_or_directory),
        path,
        "path does not exist",
    });
  }

  Result r;
  r.items.push_back(ItemResult{.source = path, .destination = {}});
  return r;
}

OpResult create_directory(const std::filesystem::path& path, const Options& options)
{
  if (cancelled(options)) {
    return Result{.items = {}, .cancelled = true};
  }

  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = {}, .destination = path});
    return r;
  }

  std::error_code ec;
  if (!std::filesystem::create_directory(path, ec)) {
    if (std::filesystem::is_directory(path)) {
      return std::unexpected(Error{
          std::make_error_code(std::errc::file_exists),
          path,
          "directory already exists",
      });
    }
    if (ec) {
      return std::unexpected(Error{ec, path, "create_directory failed"});
    }
  }

  Result r;
  r.items.push_back(ItemResult{.source = {}, .destination = path});
  return r;
}

OpResult create_file(const std::filesystem::path& path, const Options& options)
{
  if (cancelled(options)) {
    return Result{.items = {}, .cancelled = true};
  }

  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = {}, .destination = path});
    return r;
  }

  std::error_code ec;
  if (std::filesystem::exists(path, ec)) {
    return std::unexpected(Error{
        std::make_error_code(std::errc::file_exists),
        path,
        "path already exists",
    });
  }

  {
    std::ofstream out(path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!out) {
      return std::unexpected(Error{
          std::make_error_code(std::errc::io_error),
          path,
          "create_file failed",
      });
    }
  }

  Result r;
  r.items.push_back(ItemResult{.source = {}, .destination = path});
  return r;
}

OpResult create_symlink(const std::filesystem::path& target, const std::filesystem::path& link_path,
                        const Options& options)
{
  if (cancelled(options)) {
    return Result{.items = {}, .cancelled = true};
  }

  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = target, .destination = link_path});
    return r;
  }

  std::error_code ec;
  if (std::filesystem::exists(link_path, ec)) {
    if (options.conflict == ConflictPolicy::Fail) {
      return std::unexpected(Error{
          std::make_error_code(std::errc::file_exists),
          link_path,
          "link path already exists",
      });
    }
    if (options.conflict == ConflictPolicy::Skip) {
      Result r;
      r.items.push_back(ItemResult{.source = target, .destination = link_path, .skipped = true});
      return r;
    }
    if (options.conflict == ConflictPolicy::Overwrite) {
      std::filesystem::remove(link_path, ec);
      if (ec) {
        return std::unexpected(Error{ec, link_path, "failed to remove existing path for symlink"});
      }
    }
  }

  std::filesystem::create_symlink(target, link_path, ec);
  if (ec) {
    return std::unexpected(Error{ec, link_path, "create_symlink failed"});
  }

  Result r;
  r.items.push_back(ItemResult{.source = target, .destination = link_path});
  return r;
}

OpResult swap_names(const std::filesystem::path& a,
                    const std::filesystem::path& b,
                    const Options& options)
{
  namespace fs = std::filesystem;

  if (cancelled(options)) {
    return Result{.items = {}, .cancelled = true};
  }

  std::error_code ec;
  if (!fs::exists(a, ec) || !fs::exists(b, ec)) {
    return std::unexpected(Error{
        std::make_error_code(std::errc::no_such_file_or_directory),
        a,
        "both paths must exist for swap",
    });
  }

  if (!same_filesystem(a, b)) {
    return std::unexpected(Error{
        std::make_error_code(std::errc::cross_device_link),
        a,
        "cross-device swap is not supported",
    });
  }

  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = a, .destination = b});
    r.items.push_back(ItemResult{.source = b, .destination = a});
    return r;
  }

  const auto parent = a.parent_path();
  const auto tmp = unique_path(parent / (std::string(".") + a.filename().string() + ".swap"));

  fs::rename(a, tmp, ec);
  if (ec) {
    return std::unexpected(Error{ec, a, "swap: rename a -> tmp failed"});
  }
  fs::rename(b, a, ec);
  if (ec) {
    std::error_code recover;
    fs::rename(tmp, a, recover);
    return std::unexpected(Error{ec, b, "swap: rename b -> a failed"});
  }
  fs::rename(tmp, b, ec);
  if (ec) {
    return std::unexpected(Error{ec, tmp, "swap: rename tmp -> b failed"});
  }

  Result r;
  r.items.push_back(ItemResult{.source = a, .destination = b});
  r.items.push_back(ItemResult{.source = b, .destination = a});
  return r;
}

} // namespace dirops

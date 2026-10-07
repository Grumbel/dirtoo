// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirops/ops.hpp"
#include "dirops/util.hpp"

#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

namespace dirops {
namespace {

/// True if something (including a dangling symlink) occupies `p`.
bool lexists(const std::filesystem::path& p)
{
  std::error_code ec;
  return std::filesystem::exists(std::filesystem::symlink_status(p, ec));
}

/// True if `inner` is `outer` or lies below it (after resolving existing parts).
bool is_within(const std::filesystem::path& inner, const std::filesystem::path& outer)
{
  std::error_code ec;
  const auto a = std::filesystem::weakly_canonical(inner, ec);
  if (ec) {
    return false;
  }
  const auto b = std::filesystem::weakly_canonical(outer, ec);
  if (ec) {
    return false;
  }
  auto ai = a.begin();
  for (auto bi = b.begin(); bi != b.end(); ++bi, ++ai) {
    if (ai == a.end() || *ai != *bi) {
      return false;
    }
  }
  return true;
}

/// Same directory entry (resolved parent + file name), without following the
/// final component if it is a symlink.
bool same_node(const std::filesystem::path& a, const std::filesystem::path& b)
{
  std::error_code ec1;
  std::error_code ec2;
  const auto pa = std::filesystem::weakly_canonical(
      a.has_parent_path() ? a.parent_path() : std::filesystem::path{"."}, ec1);
  const auto pb = std::filesystem::weakly_canonical(
      b.has_parent_path() ? b.parent_path() : std::filesystem::path{"."}, ec2);
  return !ec1 && !ec2 && pa == pb && a.filename() == b.filename();
}

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
  if (!lexists(to)) {
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

/// Unlink a single existing destination for Overwrite.
/// Refuses directories (including non-empty trees) — never calls remove_all.
std::expected<void, Error> remove_for_overwrite(const std::filesystem::path& path)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  const auto st = fs::symlink_status(path, ec);
  if (ec) {
    return std::unexpected(Error{ec, path, "failed to stat destination for overwrite"});
  }
  if (!fs::exists(st)) {
    return {};
  }
  // Symlink-to-directory: symlink_status is symlink, not directory — safe to remove.
  if (fs::is_directory(st)) {
    return std::unexpected(Error{
        std::make_error_code(std::errc::is_a_directory),
        path,
        "refusing to overwrite directory (would delete tree); use Rename or Skip",
    });
  }
  fs::remove(path, ec);
  if (ec) {
    return std::unexpected(Error{ec, path, "failed to remove existing destination"});
  }
  return {};
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

  if (options.conflict == ConflictPolicy::Overwrite && lexists(dest)) {
    if (auto rm = remove_for_overwrite(dest); !rm) {
      return std::unexpected(rm.error());
    }
  }

  std::error_code sec;
  const std::uint64_t total = static_cast<std::uint64_t>(fs::file_size(from, sec));
  // When a progress callback is installed, stream the file in chunks so the GUI
  // can show per-file byte progress. Otherwise use the atomic-ish copy_file path.
  if (options.on_progress) {
    std::ifstream in(from, std::ios::binary);
    if (!in) {
      return std::unexpected(Error{
          std::make_error_code(std::errc::io_error),
          from,
          "failed to open source for copy",
      });
    }
    std::ofstream out(dest, std::ios::binary | std::ios::trunc);
    if (!out) {
      return std::unexpected(Error{
          std::make_error_code(std::errc::io_error),
          dest,
          "failed to open destination for copy",
      });
    }
    // Never leave a truncated copy behind after a failure.
    auto fail = [&](const std::filesystem::path& where, const char* what) {
      out.close();
      std::error_code rm_ec;
      fs::remove(dest, rm_ec);
      return std::unexpected(Error{std::make_error_code(std::errc::io_error), where, what});
    };
    constexpr std::size_t kBuf = 256 * 1024;
    std::vector<char> buf(kBuf);
    std::uint64_t done = 0;
    report_progress(options, 0, total, dest);
    while (in) {
      if (cancelled(options)) {
        out.close();
        std::error_code rm_ec;
        fs::remove(dest, rm_ec);
        Result r;
        r.cancelled = true;
        return r;
      }
      in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
      const auto n = in.gcount();
      if (n > 0) {
        out.write(buf.data(), n);
        if (!out) {
          return fail(dest, "write failed during copy");
        }
        done += static_cast<std::uint64_t>(n);
        report_progress(options, done, total > 0 ? total : done, dest);
      }
    }
    if (in.bad()) {
      return fail(from, "read failed during copy");
    }
    // close() flushes; a full disk shows up here.
    out.close();
    if (!out) {
      return fail(dest, "flush failed at end of copy");
    }
    // Preserve mtime/permissions best-effort (copy_file would do this).
    std::error_code cec;
    fs::last_write_time(dest, fs::last_write_time(from, cec), cec);
    fs::permissions(dest, fs::status(from, cec).permissions(),
                    fs::perm_options::replace, cec);
  } else {
    std::error_code ec;
    fs::copy_file(from, dest, fs::copy_options::none, ec);
    if (ec) {
      return std::unexpected(Error{ec, from, "copy_file failed"});
    }
    std::uint64_t size = total;
    if (size == 0) {
      std::error_code zec;
      size = static_cast<std::uint64_t>(fs::file_size(dest, zec));
    }
    report_progress(options, size, size, dest);
  }

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
  fs::directory_iterator it(from, ec);
  if (ec) {
    // An unreadable directory must not look like an empty one (a move would
    // then report success).
    return std::unexpected(Error{ec, from, "cannot read directory"});
  }
  for (; it != fs::directory_iterator{}; it.increment(ec)) {
    if (ec) {
      return std::unexpected(Error{ec, from, "directory iteration failed"});
    }
    const auto& entry = *it;
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
        auto resolved_link = resolve_destination(dst, options);
        if (!resolved_link) {
          return std::unexpected(resolved_link.error());
        }
        if (resolved_link->skipped) {
          acc.items.push_back(ItemResult{.source = src, .destination = dst, .skipped = true});
          continue;
        }
        if (options.conflict == ConflictPolicy::Overwrite && lexists(dst)) {
          if (auto rm = remove_for_overwrite(dst); !rm) {
            return std::unexpected(rm.error());
          }
        }
        fs::create_symlink(target, resolved_link->path, lec);
        if (lec) {
          return std::unexpected(Error{lec, resolved_link->path, "create_symlink failed"});
        }
        acc.items.push_back(ItemResult{.source = src, .destination = resolved_link->path});
        continue;
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
      if (file_result->cancelled) {
        acc.cancelled = true;
        return acc;
      }
    } else {
      // FIFOs, sockets, devices: refuse instead of silently dropping them
      // (a cross-device move would then delete the source).
      return std::unexpected(Error{
          std::make_error_code(std::errc::operation_not_supported),
          src,
          "unsupported file type (not a regular file, directory or symlink)",
      });
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
  // symlink_status: a dangling symlink is a valid source.
  const auto from_status = fs::symlink_status(from, ec);
  if (ec || !fs::exists(from_status)) {
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

  // Overwrite would unlink the destination first — which is the source.
  if (options.conflict == ConflictPolicy::Overwrite && same_node(from, dest)) {
    return std::unexpected(Error{
        std::make_error_code(std::errc::invalid_argument),
        from,
        "source and destination are the same file",
    });
  }

  if (fs::is_symlink(from_status)) {
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
      if (options.conflict == ConflictPolicy::Overwrite && lexists(resolved->path)) {
        if (auto rm = remove_for_overwrite(resolved->path); !rm) {
          return std::unexpected(rm.error());
        }
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

  if (fs::is_directory(from_status)) {
    if (is_within(dest, from)) {
      return std::unexpected(Error{
          std::make_error_code(std::errc::invalid_argument),
          from,
          "cannot copy a directory into itself",
      });
    }
    Result acc;
    return copy_directory_recursive(from, dest, options, acc);
  }

  if (!fs::is_regular_file(from_status)) {
    return std::unexpected(Error{
        std::make_error_code(std::errc::operation_not_supported),
        from,
        "unsupported file type (not a regular file, directory or symlink)",
    });
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
  const auto from_status = fs::symlink_status(from, ec);
  if (ec || !fs::exists(from_status)) {
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

  // Moving a node onto itself is a no-op. Without this, Overwrite would unlink
  // the destination — which is the source — first.
  if (same_node(from, dest)) {
    Result r;
    r.items.push_back(ItemResult{.source = from, .destination = dest, .skipped = true});
    return r;
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

  if (fs::is_directory(from_status) && is_within(dest, from)) {
    return std::unexpected(Error{
        std::make_error_code(std::errc::invalid_argument),
        from,
        "cannot move a directory into itself",
    });
  }

  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = from, .destination = dest});
    return r;
  }

  // Clear an existing file/symlink destination up front (directories are
  // refused), so both the rename and the copy path see a free name.
  if (options.conflict == ConflictPolicy::Overwrite && lexists(dest)) {
    if (auto rm = remove_for_overwrite(dest); !rm) {
      return std::unexpected(rm.error());
    }
  }

  // Prefer atomic rename when on the same filesystem.
  if (same_filesystem(from, dest)) {
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

  // Cross-device: copy to the (now free) destination, then remove the source.
  // The source is only removed after a copy that completed without error and
  // without cancellation; otherwise the partial copy is discarded instead.
  Options copy_opts = options;
  copy_opts.conflict = ConflictPolicy::Fail;
  auto copied = copy_path(from, dest, copy_opts);
  if (!copied || copied->cancelled) {
    std::error_code rm_ec;
    fs::remove_all(dest, rm_ec);  // dest did not exist before this call
    return copied;
  }

  fs::remove_all(from, ec);
  if (ec) {
    return std::unexpected(Error{ec, from, "copied but failed to remove source"});
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
      && lexists(resolved->path)
      && resolved->path != from) {
    if (auto rm = remove_for_overwrite(resolved->path); !rm) {
      return std::unexpected(rm.error());
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

  // remove_all() has `rm -rf` semantics. Like GNU rm, refuse the targets that
  // would wipe far more than the caller can mean: nothing, the filesystem
  // root, and "." / ".." (remove_all(".") deletes the contents of the current
  // directory and only then fails).
  {
    auto last = path;
    while (last.has_relative_path() && last.filename().empty()) {
      last = last.parent_path();  // strip trailing slashes: "dir/" -> "dir"
    }
    const auto name = last.filename();
    if (path.empty() || !path.has_relative_path() || name == "." || name == "..") {
      return std::unexpected(Error{
          std::make_error_code(std::errc::invalid_argument),
          path,
          "refusing to remove an empty path, the filesystem root, '.' or '..'",
      });
    }
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
  namespace fs = std::filesystem;

  if (cancelled(options)) {
    return Result{.items = {}, .cancelled = true};
  }

  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = target, .destination = link_path});
    return r;
  }

  std::error_code ec;
  fs::path final_path = link_path;
  if (lexists(link_path)) {
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
      if (auto rm = remove_for_overwrite(link_path); !rm) {
        return std::unexpected(rm.error());
      }
    } else if (options.conflict == ConflictPolicy::Rename) {
      final_path = unique_path(link_path);
    }
  }

  fs::create_symlink(target, final_path, ec);
  if (ec) {
    return std::unexpected(Error{ec, final_path, "create_symlink failed"});
  }

  Result r;
  r.items.push_back(ItemResult{.source = target, .destination = final_path});
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
  if (!lexists(a) || !lexists(b)) {
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
    // `a` already holds b's old content; the original a survives at `tmp`.
    return std::unexpected(Error{ec, tmp, "swap: rename tmp -> b failed (original of a kept at this path)"});
  }

  Result r;
  r.items.push_back(ItemResult{.source = a, .destination = b});
  r.items.push_back(ItemResult{.source = b, .destination = a});
  return r;
}

OpResult set_permissions(const std::filesystem::path& path,
                         std::uint32_t mode,
                         const Options& options)
{
  if (cancelled(options)) {
    return Result{.items = {}, .cancelled = true};
  }

  if (options.dry_run) {
    Result r;
    r.items.push_back(ItemResult{.source = path, .destination = path});
    return r;
  }

  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return std::unexpected(Error{
        ec ? ec : std::make_error_code(std::errc::no_such_file_or_directory),
        path,
        "path does not exist",
    });
  }

  // Keep only permission / special bits; never pass type bits to permissions().
  const auto perms = static_cast<std::filesystem::perms>(mode & 07777u);
  std::filesystem::permissions(path, perms, std::filesystem::perm_options::replace, ec);
  if (ec) {
    return std::unexpected(Error{ec, path, "set_permissions failed"});
  }

  Result r;
  r.items.push_back(ItemResult{.source = path, .destination = path});
  return r;
}

} // namespace dirops

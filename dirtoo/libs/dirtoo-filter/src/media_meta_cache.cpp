// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/media_meta_cache.hpp"

#include <sqlite3.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <sys/stat.h>
#include <thread>
#include <vector>

namespace dirtoo::filter {
namespace {

constexpr int k_db_user_version = 2;
constexpr int k_worker_count = 2;

} // namespace

// probe_media in media_probe.cpp is the ffprobe implementation — we need a
// non-caching entry. Declare internal helper by calling the existing probe once
// after clearing is awkward; instead re-use probe_media but insert our disk layer
// around it from resolve path only.

FileFingerprint fingerprint_file(const std::filesystem::path& path)
{
  FileFingerprint fp;
  struct ::stat st {};
  if (::stat(path.c_str(), &st) != 0) {
    return fp;
  }
  fp.valid = true;
  fp.size = static_cast<std::uint64_t>(st.st_size);
#if defined(st_mtim)
  fp.mtime_ns = static_cast<std::int64_t>(st.st_mtim.tv_sec) * 1'000'000'000LL
                + static_cast<std::int64_t>(st.st_mtim.tv_nsec);
#elif defined(__APPLE__)
  fp.mtime_ns = static_cast<std::int64_t>(st.st_mtimespec.tv_sec) * 1'000'000'000LL
                + static_cast<std::int64_t>(st.st_mtimespec.tv_nsec);
#else
  fp.mtime_ns = static_cast<std::int64_t>(st.st_mtime) * 1'000'000'000LL;
#endif
  return fp;
}

struct MediaMetaCache::Impl {
  mutable std::mutex mu;
  std::map<std::string, std::optional<MediaInfo>> memory; // nullopt = negative cache
  std::map<std::string, bool> in_flight;

  sqlite3* db = nullptr;
  std::mutex db_mu;

  std::atomic<std::uint64_t> generation{1};
  std::atomic<bool> stopping{false};

  struct Job {
    std::string path;
    std::uint64_t generation = 0;
    ReadyCallback cb;
  };
  std::mutex queue_mu;
  std::condition_variable queue_cv;
  std::deque<Job> queue;
  std::vector<std::thread> workers;

  void ensure_workers()
  {
    if (!workers.empty()) {
      return;
    }
    for (int i = 0; i < k_worker_count; ++i) {
      workers.emplace_back([this] { worker_loop(); });
    }
  }

  void worker_loop()
  {
    while (true) {
      Job job;
      {
        std::unique_lock lock(queue_mu);
        queue_cv.wait(lock, [this] { return stopping.load() || !queue.empty(); });
        if (stopping.load() && queue.empty()) {
          return;
        }
        job = std::move(queue.front());
        queue.pop_front();
      }
      auto result = resolve_job(job.path);
      {
        std::lock_guard lock(mu);
        memory[job.path] = result;
        in_flight.erase(job.path);
      }
      if (job.cb) {
        job.cb(job.path, result, job.generation);
      }
    }
  }

  std::optional<MediaInfo> resolve_job(const std::string& path_str)
  {
    const std::filesystem::path path{path_str};
    const auto fp = fingerprint_file(path);
    if (!fp.valid) {
      return std::nullopt;
    }

    if (auto from_db = db_get(path_str, fp)) {
      return *from_db;
    }

    // Miss or stale: probe then store.
    // Call the real ffprobe path (existing probe_media has process memory cache —
    // clear is not needed; we still want disk persistence).
    auto info = probe_media_raw(path);
    db_put(path_str, fp, info);
    return info;
  }

  bool open_db(const std::filesystem::path& db_path)
  {
    std::lock_guard lock(db_mu);
    if (db != nullptr) {
      return true;
    }
    std::error_code ec;
    std::filesystem::create_directories(db_path.parent_path(), ec);
    if (sqlite3_open(db_path.string().c_str(), &db) != SQLITE_OK) {
      if (db != nullptr) {
        sqlite3_close(db);
        db = nullptr;
      }
      return false;
    }
    char* err = nullptr;
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &err);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, &err);
    const char* schema = R"sql(
      CREATE TABLE IF NOT EXISTS media_meta (
        path TEXT PRIMARY KEY NOT NULL,
        mtime_ns INTEGER NOT NULL,
        size INTEGER NOT NULL,
        width INTEGER,
        height INTEGER,
        duration_ms INTEGER,
        framerate REAL,
        probed_at INTEGER NOT NULL
      );
    )sql";
    if (sqlite3_exec(db, schema, nullptr, nullptr, &err) != SQLITE_OK) {
      sqlite3_free(err);
      sqlite3_close(db);
      db = nullptr;
      return false;
    }
    // v2: pages + file_count (idempotent ADD COLUMN)
    sqlite3_exec(db, "ALTER TABLE media_meta ADD COLUMN pages INTEGER;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "ALTER TABLE media_meta ADD COLUMN file_count INTEGER;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, ("PRAGMA user_version=" + std::to_string(k_db_user_version) + ";").c_str(),
                 nullptr, nullptr, &err);
    return true;
  }

  /// Returns nullopt if no row; returns optional<optional<MediaInfo>> — use has_value for hit.
  std::optional<std::optional<MediaInfo>> db_get(const std::string& path, const FileFingerprint& fp)
  {
    std::lock_guard lock(db_mu);
    if (db == nullptr) {
      return std::nullopt;
    }
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT mtime_ns, size, width, height, duration_ms, framerate, pages, file_count FROM media_meta WHERE path=?1;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<std::optional<MediaInfo>> out;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      const auto mtime = sqlite3_column_int64(stmt, 0);
      const auto size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 1));
      if (mtime == fp.mtime_ns && size == fp.size) {
        MediaInfo info;
        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
          info.width = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 2));
        }
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
          info.height = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 3));
        }
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
          info.duration_ms = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 4));
        }
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
          info.framerate = sqlite3_column_double(stmt, 5);
        }
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
          info.pages = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 6));
        }
        if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
          info.file_count = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 7));
        }
        if (info.width || info.height || info.duration_ms || info.framerate || info.pages
            || info.file_count) {
          out = info;
        } else {
          out = std::optional<MediaInfo>{std::nullopt}; // negative hit
        }
      }
    }
    sqlite3_finalize(stmt);
    return out;
  }

  void db_put(const std::string& path, const FileFingerprint& fp, const std::optional<MediaInfo>& info)
  {
    std::lock_guard lock(db_mu);
    if (db == nullptr) {
      return;
    }
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT OR REPLACE INTO media_meta"
        "(path,mtime_ns,size,width,height,duration_ms,framerate,pages,file_count,probed_at)"
        " VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      return;
    }
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, fp.mtime_ns);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(fp.size));
    if (info && info->width) {
      sqlite3_bind_int(stmt, 4, static_cast<int>(*info->width));
    } else {
      sqlite3_bind_null(stmt, 4);
    }
    if (info && info->height) {
      sqlite3_bind_int(stmt, 5, static_cast<int>(*info->height));
    } else {
      sqlite3_bind_null(stmt, 5);
    }
    if (info && info->duration_ms) {
      sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(*info->duration_ms));
    } else {
      sqlite3_bind_null(stmt, 6);
    }
    if (info && info->framerate) {
      sqlite3_bind_double(stmt, 7, *info->framerate);
    } else {
      sqlite3_bind_null(stmt, 7);
    }
    if (info && info->pages) {
      sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(*info->pages));
    } else {
      sqlite3_bind_null(stmt, 8);
    }
    if (info && info->file_count) {
      sqlite3_bind_int64(stmt, 9, static_cast<sqlite3_int64>(*info->file_count));
    } else {
      sqlite3_bind_null(stmt, 9);
    }
    sqlite3_bind_int64(stmt, 10, static_cast<sqlite3_int64>(now));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }

  void close_db()
  {
    std::lock_guard lock(db_mu);
    if (db != nullptr) {
      sqlite3_close(db);
      db = nullptr;
    }
  }
};

MediaMetaCache::MediaMetaCache()
    : impl_(std::make_unique<Impl>())
{
}

MediaMetaCache::~MediaMetaCache()
{
  close();
}

MediaMetaCache& MediaMetaCache::instance()
{
  static MediaMetaCache cache;
  return cache;
}

std::filesystem::path MediaMetaCache::default_db_path()
{
  const char* xdg = std::getenv("XDG_CACHE_HOME");
  std::filesystem::path base;
  if (xdg != nullptr && xdg[0] != '\0') {
    base = xdg;
  } else {
    const char* home = std::getenv("HOME");
    base = home != nullptr ? std::filesystem::path(home) / ".cache" : std::filesystem::path("/tmp");
  }
  return base / "dirtoo" / "meta.sqlite";
}

void MediaMetaCache::open(const std::filesystem::path& db_path)
{
  const auto path = db_path.empty() ? default_db_path() : db_path;
  impl_->open_db(path);
  impl_->ensure_workers();
}

void MediaMetaCache::close()
{
  {
    std::lock_guard lock(impl_->queue_mu);
    impl_->stopping = true;
  }
  impl_->queue_cv.notify_all();
  for (auto& t : impl_->workers) {
    if (t.joinable()) {
      t.join();
    }
  }
  impl_->workers.clear();
  impl_->close_db();
  impl_->stopping = false;
}

std::optional<MediaInfo> MediaMetaCache::try_get(const std::filesystem::path& path) const
{
  std::lock_guard lock(impl_->mu);
  const auto it = impl_->memory.find(path.string());
  if (it == impl_->memory.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool MediaMetaCache::is_negative(const std::filesystem::path& path) const
{
  std::lock_guard lock(impl_->mu);
  const auto it = impl_->memory.find(path.string());
  return it != impl_->memory.end() && !it->second.has_value();
}

void MediaMetaCache::request(const std::filesystem::path& path, std::uint64_t generation,
                             ReadyCallback on_ready)
{
  open(); // ensure db + workers
  const std::string key = path.string();
  {
    std::lock_guard lock(impl_->mu);
    if (const auto it = impl_->memory.find(key); it != impl_->memory.end()) {
      if (on_ready) {
        on_ready(key, it->second, generation);
      }
      return;
    }
    if (impl_->in_flight.contains(key)) {
      return;
    }
    impl_->in_flight.emplace(key, true);
  }
  {
    std::lock_guard lock(impl_->queue_mu);
    impl_->queue.push_back(Impl::Job{key, generation, std::move(on_ready)});
  }
  impl_->queue_cv.notify_one();
}

std::uint64_t MediaMetaCache::bump_generation()
{
  return ++impl_->generation;
}

std::uint64_t MediaMetaCache::generation() const
{
  return impl_->generation.load();
}

std::optional<MediaInfo> resolve_media_cached(const std::filesystem::path& path)
{
  auto& cache = MediaMetaCache::instance();
  cache.open();
  if (auto hit = cache.try_get(path)) {
    return hit;
  }
  // Synchronous path for CLI: run resolve on this thread via request barrier.
  std::mutex m;
  std::condition_variable cv;
  bool done = false;
  std::optional<MediaInfo> result;
  cache.request(path, 0, [&](const std::string&, std::optional<MediaInfo> info, std::uint64_t) {
    std::lock_guard lock(m);
    result = std::move(info);
    done = true;
    cv.notify_one();
  });
  std::unique_lock lock(m);
  cv.wait(lock, [&] { return done; });
  return result;
}

} // namespace dirtoo::filter

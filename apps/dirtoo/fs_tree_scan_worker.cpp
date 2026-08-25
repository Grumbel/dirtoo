// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fs_tree_scan_worker.hpp"

#include "dirtoo/tree/fs_tree_size_store.hpp"

#include <exception>
#include <filesystem>
#include <mutex>

namespace dirtoo::app {
namespace {

std::mutex g_cache_mutex;
dirtoo::tree::FsTreeCache* g_cache = nullptr;
dirtoo::tree::FsTreeSizeStore* g_size_store = nullptr;

} // namespace

dirtoo::tree::FsTreeCache& app_fs_tree_cache()
{
  std::lock_guard lock(g_cache_mutex);
  if (g_cache == nullptr) {
    g_cache = new dirtoo::tree::FsTreeCache();
    g_size_store = new dirtoo::tree::FsTreeSizeStore();
    if (g_size_store->open(dirtoo::tree::FsTreeSizeStore::default_db_path())) {
      g_cache->set_size_store(g_size_store);
      g_cache->load_persisted_sizes();
    }
  }
  return *g_cache;
}

FsTreeScanWorker::FsTreeScanWorker(QObject* parent)
    : QObject(parent)
{
}

void FsTreeScanWorker::cancel()
{
  const quint64 cur = cancel_generation_.load(std::memory_order_relaxed);
  cancel_generation_.store(cur ^ ~quint64{0}, std::memory_order_relaxed);
}

void FsTreeScanWorker::scan(const QString& path, quint64 generation, bool include_hidden,
                            bool cross_device, int max_depth)
{
  cancel_generation_.store(generation, std::memory_order_relaxed);

  try {
    const std::filesystem::path root{path.toUtf8().constData()};
    dirtoo::tree::ScanOptions opts;
    opts.include_hidden = include_hidden;
    opts.cross_device = cross_device;
    if (max_depth >= 0) {
      opts.max_depth = static_cast<std::uint32_t>(max_depth);
    }

    std::atomic_bool cancel_flag{false};
    std::uint64_t nodes_ready = 0;
    dirtoo::tree::ScanCallbacks cb;
    cb.progress = [this, generation, &cancel_flag](std::uint64_t nodes_seen,
                                                   const std::filesystem::path& current) {
      if (cancel_generation_.load(std::memory_order_relaxed) != generation) {
        cancel_flag.store(true, std::memory_order_relaxed);
        return;
      }
      emit progress(generation, nodes_seen,
                    QString::fromUtf8(current.u8string().c_str()));
    };
    cb.node_ready = [this, generation, &nodes_ready](const std::filesystem::path&,
                                                     std::uint64_t, dirtoo::tree::FsTreeNodeKind) {
      if (cancel_generation_.load(std::memory_order_relaxed) != generation) {
        return;
      }
      ++nodes_ready;
      // Throttle UI: every 16 completed nodes (dirs finalize late; still useful).
      if (nodes_ready == 1 || (nodes_ready % 16) == 0) {
        emit partial(generation, nodes_ready);
      }
    };

    auto tree = app_fs_tree_cache().scan(root, opts, &cancel_flag, std::move(cb));

    if (cancel_generation_.load(std::memory_order_relaxed) != generation) {
      return;
    }

    const auto key = dirtoo::tree::FsTreeCache::path_key(root);
    emit finished(generation, QString::fromUtf8(key.c_str()), tree->total_size(),
                  static_cast<int>(tree->state()));
  } catch (const std::exception& ex) {
    if (cancel_generation_.load(std::memory_order_relaxed) != generation) {
      return;
    }
    emit failed(generation, QString::fromUtf8(ex.what()));
  } catch (...) {
    if (cancel_generation_.load(std::memory_order_relaxed) != generation) {
      return;
    }
    emit failed(generation, QStringLiteral("Folder size scan failed"));
  }
}

} // namespace dirtoo::app

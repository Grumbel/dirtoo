// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tag_job.hpp"
#include "hash_service.hpp"

#include "archive_member_cache.hpp"

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/hash/hash_file.hpp"
#include "dirtoo/tags/tag_store.hpp"

#include <QThread>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <memory>
#include <utility>

namespace dirtoo::app {
namespace {

class TagWorker : public QObject {
  Q_OBJECT
public:
  TagWorker(std::vector<dirtoo::fs::FileInfo> files, QStringList tags, TagJob::Mode mode,
            std::shared_ptr<std::atomic_bool> cancel)
      : files_(std::move(files))
      , tags_(std::move(tags))
      , mode_(mode)
      , cancel_(std::move(cancel))
  {
  }

public slots:
  void run()
  {
    auto& hashes = HashService::instance();
    dirtoo::tags::TagStore tags;
    std::string err;
    if (!hashes.ensure_open(&err) || !tags.open(dirtoo::tags::TagStore::default_path(), &err)) {
      emit failed(QString::fromStdString(err));
      emit finished(0, 0, {});
      return;
    }

    int tagged = 0;
    int skipped = 0;
    QStringList problems;
    const int total = static_cast<int>(files_.size());
    constexpr int kScale = 1000;
    const int total_scaled = std::max(1, total) * kScale;
    auto emit_file_progress = [&](int index, double frac, const QString& name) {
      frac = std::clamp(frac, 0.0, 1.0);
      const int done = index * kScale + static_cast<int>(frac * (kScale - 1));
      emit progress(done, total_scaled, name);
    };
    // Share extract cache with thumbnails so re-tagging does not re-extract.
    const auto cache_root = archive_member_cache_root("dirtoo-archive-thumbs");

    for (int i = 0; i < total; ++i) {
      if (cancel_ && cancel_->load()) {
        break;
      }
      const auto& fi = files_[static_cast<std::size_t>(i)];
      const QString display = QString::fromStdString(fi.basename());
      emit_file_progress(i, 0.0, display);

      std::string key;
      dirtoo::hash::HashError herr;

      if (fi.location().is_archive()) {
        key = fi.location().as_url();
        if (!hashes.get_full(key)) {
          const auto member = fi.location().entry_path();
          if (member.empty()) {
            ++skipped;
            problems << QStringLiteral("%1: cannot tag archive root; select a member file")
                            .arg(display);
            continue;
          }
          const auto archive_file = fi.location().as_path();
          const auto dest_dir = archive_member_dest_dir(cache_root, archive_file);
          auto extracted = ensure_archive_member_extracted(archive_file, member, dest_dir);
          if (!extracted) {
            ++skipped;
            problems << QStringLiteral("%1: archive extract failed").arg(display);
            continue;
          }
          dirtoo::hash::HashOptions hops;
          hops.should_cancel = [this]() { return cancel_ && cancel_->load(); };
          hops.on_progress = [this, i, total, display, &emit_file_progress](std::uint64_t rd, std::uint64_t sz) {
            (void)this;
            if (sz > 0) {
              emit_file_progress(i, static_cast<double>(rd) / static_cast<double>(sz), display);
            }
          };
          auto digests = dirtoo::hash::hash_file(*extracted, hops, &herr);
          if (!digests) {
            ++skipped;
            problems << QStringLiteral("%1: %2").arg(display, QString::fromStdString(herr.message));
            continue;
          }
          digests->size = fi.size() != 0 ? fi.size() : digests->size;
          hashes.put_full(key, *digests);
        }
      } else {
        std::error_code ec;
        const auto abs = std::filesystem::absolute(fi.path(), ec);
        key = ec ? fi.path().string() : abs.lexically_normal().string();
        dirtoo::hash::HashOptions hops;
        hops.should_cancel = [this]() { return cancel_ && cancel_->load(); };
        hops.on_progress = [i, display, &emit_file_progress](std::uint64_t rd, std::uint64_t sz) {
          if (sz > 0) {
            emit_file_progress(i, static_cast<double>(rd) / static_cast<double>(sz), display);
          }
        };
        if (!hashes.ensure_full(abs, key, false, &herr, hops)) {
          ++skipped;
          problems << QStringLiteral("%1: %2").arg(display, QString::fromStdString(herr.message));
          continue;
        }
      }

      std::string e;
      auto id = hashes.with_store(
          [&](dirtoo::hash::ChecksumStore& store) { return tags.resolve_path(store, key, &e); });
      if (!id) {
        ++skipped;
        problems << QStringLiteral("%1: %2").arg(display, QString::fromStdString(e));
        continue;
      }
      bool any = false;
      for (const QString& tg : tags_) {
        std::string e2;
        const bool ok = (mode_ == TagJob::Mode::Remove)
                            ? tags.remove_tag_from_file(*id, tg.toStdString(), &e2)
                            : tags.add_tag_to_file(*id, tg.toStdString(), &e2);
        if (!ok) {
          problems << QStringLiteral("%1 [%2]: %3")
                          .arg(display, tg, QString::fromStdString(e2));
          continue;
        }
        any = true;
      }
      if (any) {
        ++tagged;
      } else {
        ++skipped;
      }
    }

    emit progress(total_scaled, total_scaled, {});
    emit finished(tagged, skipped, problems);
  }

signals:
  void progress(int done, int total, const QString& name);
  void finished(int tagged, int skipped, const QStringList& problems);
  void failed(const QString& message);

private:
  std::vector<dirtoo::fs::FileInfo> files_;
  QStringList tags_;
  TagJob::Mode mode_ = TagJob::Mode::Add;
  std::shared_ptr<std::atomic_bool> cancel_;
};

} // namespace

struct TagJob::Impl {
  QThread* thread = nullptr;
  TagWorker* worker = nullptr;
  /// Shared with the worker so cancel never needs QMetaObject on a dying QObject.
  std::shared_ptr<std::atomic_bool> cancel = std::make_shared<std::atomic_bool>(false);
};

TagJob::TagJob(std::vector<dirtoo::fs::FileInfo> files, QStringList tags, Mode mode,
               QObject* parent)
    : QObject(parent)
    , files_(std::move(files))
    , tags_(std::move(tags))
    , mode_(mode)
    , impl_(new Impl)
{
}

TagJob::~TagJob()
{
  cancel();
  if (impl_ != nullptr && impl_->thread != nullptr) {
    impl_->thread->quit();
    impl_->thread->wait(5000);
  }
  delete impl_;
  impl_ = nullptr;
}

void TagJob::start()
{
  if (impl_ == nullptr || impl_->thread != nullptr) {
    return;
  }
  if (impl_->cancel) {
    impl_->cancel->store(false);
  }
  impl_->worker = new TagWorker(std::move(files_), tags_, mode_, impl_->cancel);
  impl_->thread = new QThread(this);
  impl_->worker->moveToThread(impl_->thread);
  connect(impl_->thread, &QThread::started, impl_->worker, &TagWorker::run);
  connect(impl_->worker, &TagWorker::progress, this, &TagJob::progress);
  connect(impl_->worker, &TagWorker::finished, this, &TagJob::finished);
  connect(impl_->worker, &TagWorker::failed, this, &TagJob::failed);
  connect(impl_->worker, &TagWorker::finished, impl_->thread, &QThread::quit);
  connect(impl_->worker, &TagWorker::failed, impl_->thread, &QThread::quit);
  connect(impl_->thread, &QThread::finished, impl_->worker, &QObject::deleteLater);
  connect(impl_->thread, &QThread::finished, this, [this] {
    if (impl_ != nullptr) {
      impl_->thread = nullptr;
      impl_->worker = nullptr;
    }
  });
  impl_->thread->start();
}

void TagJob::cancel()
{
  // Only touch the shared atomic — never invokeMethod on the worker (it may
  // already be deleteLater'd when TagJob is destroyed after finished).
  if (impl_ != nullptr && impl_->cancel) {
    impl_->cancel->store(true);
  }
}

} // namespace dirtoo::app

#include "tag_job.moc"

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tag_job.hpp"

#include "archive_member_cache.hpp"

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/hash/hash_file.hpp"
#include "dirtoo/tags/tag_store.hpp"

#include <QThread>

#include <atomic>
#include <filesystem>
#include <memory>
#include <utility>

namespace dirtoo::app {
namespace {

class TagWorker : public QObject {
  Q_OBJECT
public:
  TagWorker(std::vector<dirtoo::fs::FileInfo> files, QString tag,
            std::shared_ptr<std::atomic_bool> cancel)
      : files_(std::move(files))
      , tag_(std::move(tag))
      , cancel_(std::move(cancel))
  {
  }

public slots:
  void run()
  {
    dirtoo::hash::ChecksumStore checksums;
    dirtoo::tags::TagStore tags;
    std::string err;
    if (!checksums.open(dirtoo::hash::ChecksumStore::default_path(), &err)
        || !tags.open(dirtoo::tags::TagStore::default_path(), &err)) {
      emit failed(QString::fromStdString(err));
      emit finished(0, 0, {});
      return;
    }

    int tagged = 0;
    int skipped = 0;
    QStringList problems;
    const int total = static_cast<int>(files_.size());
    // Share extract cache with thumbnails so re-tagging does not re-extract.
    const auto cache_root = archive_member_cache_root("dirtoo-archive-thumbs");

    for (int i = 0; i < total; ++i) {
      if (cancel_ && cancel_->load()) {
        break;
      }
      const auto& fi = files_[static_cast<std::size_t>(i)];
      const QString display = QString::fromStdString(fi.basename());
      emit progress(i, total, display);

      std::string key;
      dirtoo::hash::HashError herr;

      if (fi.location().is_archive()) {
        key = fi.location().as_url();
        if (!checksums.get(key)) {
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
          auto digests = dirtoo::hash::hash_file(*extracted, {}, &herr);
          if (!digests) {
            ++skipped;
            problems << QStringLiteral("%1: %2").arg(display, QString::fromStdString(herr.message));
            continue;
          }
          digests->size = fi.size() != 0 ? fi.size() : digests->size;
          checksums.put(key, *digests);
        }
      } else {
        std::error_code ec;
        const auto abs = std::filesystem::absolute(fi.path(), ec);
        key = ec ? fi.path().string() : abs.lexically_normal().string();
        if (!checksums.ensure(abs, key, false, &herr)) {
          ++skipped;
          problems << QStringLiteral("%1: %2").arg(display, QString::fromStdString(herr.message));
          continue;
        }
      }

      std::string e;
      auto id = tags.resolve_path(checksums, key, &e);
      if (!id || !tags.add_tag_to_file(*id, tag_.toStdString(), &e)) {
        ++skipped;
        problems << QStringLiteral("%1: %2").arg(display, QString::fromStdString(e));
        continue;
      }
      ++tagged;
    }

    emit progress(total, total, {});
    emit finished(tagged, skipped, problems);
  }

signals:
  void progress(int done, int total, const QString& name);
  void finished(int tagged, int skipped, const QStringList& problems);
  void failed(const QString& message);

private:
  std::vector<dirtoo::fs::FileInfo> files_;
  QString tag_;
  std::shared_ptr<std::atomic_bool> cancel_;
};

} // namespace

struct TagJob::Impl {
  QThread* thread = nullptr;
  TagWorker* worker = nullptr;
  /// Shared with the worker so cancel never needs QMetaObject on a dying QObject.
  std::shared_ptr<std::atomic_bool> cancel = std::make_shared<std::atomic_bool>(false);
};

TagJob::TagJob(std::vector<dirtoo::fs::FileInfo> files, QString tag, QObject* parent)
    : QObject(parent)
    , files_(std::move(files))
    , tag_(std::move(tag))
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
  impl_->worker = new TagWorker(std::move(files_), tag_, impl_->cancel);
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

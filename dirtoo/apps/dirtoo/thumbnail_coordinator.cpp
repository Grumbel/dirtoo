// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thumbnail_coordinator.hpp"

#include "dirtoo/fs/location.hpp"

namespace dirtoo::app {

ThumbnailCoordinator::ThumbnailCoordinator(QObject* parent)
    : QObject(parent)
{
}

ThumbnailCoordinator::~ThumbnailCoordinator()
{
  shutdown();
}

void ThumbnailCoordinator::cancel_all()
{
  thumbnailer_.cancel_all();
}

void ThumbnailCoordinator::clear_aliases()
{
  thumb_alias_.clear();
}

void ThumbnailCoordinator::request_many(const std::vector<fs::Location>& locs,
                                       const QStringList& mimes)
{
  if (locs.empty()) {
    return;
  }
  thumbnailer_.request_many(locs, mimes, QStringLiteral("large"));
}

void ThumbnailCoordinator::setup_dir_worker()
{
  if (dir_thumb_worker_ != nullptr) {
    return;
  }
  dir_thumb_worker_ = new DirectoryThumbnailWorker;
  dir_thumb_thread_ = new QThread(this);
  dir_thumb_worker_->moveToThread(dir_thumb_thread_);
  connect(dir_thumb_thread_, &QThread::finished, dir_thumb_worker_, &QObject::deleteLater);
  dir_thumb_thread_->start();
}

void ThumbnailCoordinator::shutdown()
{
  cancel_all();
  clear_aliases();
  if (dir_thumb_thread_ != nullptr) {
    dir_thumb_thread_->quit();
    dir_thumb_thread_->wait(3000);
    dir_thumb_thread_ = nullptr;
    dir_thumb_worker_ = nullptr;
  }
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/file_info.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

#include <vector>

namespace dirtoo::app {

/// Background tag job: hash (and archive extract) off the GUI thread, then
/// write tags via TagStore. Parent shows progress and handles finished/failed.
class TagJob : public QObject {
  Q_OBJECT
public:
  TagJob(std::vector<dirtoo::fs::FileInfo> files, QStringList tags, QObject* parent = nullptr);
  ~TagJob() override;

  void start();
  void cancel();

signals:
  void progress(int done, int total, const QString& name);
  void finished(int tagged, int skipped, const QStringList& problems);
  void failed(const QString& message);

private:
  std::vector<dirtoo::fs::FileInfo> files_;
  QStringList tags_;
  struct Impl;
  Impl* impl_ = nullptr;
};

} // namespace dirtoo::app

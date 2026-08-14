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
/// add or remove tags via TagStore. Parent shows progress and handles finished/failed.
class TagJob : public QObject {
  Q_OBJECT
public:
  enum class Mode { Add, Remove };

  TagJob(std::vector<dirtoo::fs::FileInfo> files, QStringList tags, Mode mode = Mode::Add,
         QObject* parent = nullptr);
  ~TagJob() override;

  void start();
  void cancel();

signals:
  /// @p done/@p total are in milli-files (file_index*1000 + in-file fraction).
  void progress(int done, int total, const QString& name);
  /// @p changed = files that gained/lost at least one requested tag.
  void finished(int changed, int skipped, const QStringList& problems);
  void failed(const QString& message);

private:
  std::vector<dirtoo::fs::FileInfo> files_;
  QStringList tags_;
  Mode mode_ = Mode::Add;
  struct Impl;
  Impl* impl_ = nullptr;
};

} // namespace dirtoo::app

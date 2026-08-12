// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QStringList>

#include <filesystem>
#include <vector>

class QProgressBar;
class QPushButton;
class QTreeWidget;
class QThread;

namespace dirtoo::app {

/// Modeless checksum dialog. Owns its worker thread (no stack-lifetime captures).
class ChecksumDialog : public QDialog {
  Q_OBJECT
public:
  explicit ChecksumDialog(QStringList paths, QWidget* parent = nullptr);
  ~ChecksumDialog() override;

private slots:
  void start_compute();
  void start_cached_only();
  void cancel_job();
  void copy_sha256();
  void clear_cache_entries();
  void on_row(const QString& path, const QString& status, const QString& crc32, const QString& md5,
              const QString& sha1, const QString& sha256, const QString& error);
  void on_progress(int done, int total);
  void on_finished();
  void on_failed(const QString& message);

private:
  void start_job(bool refresh, bool cached_only);
  void stop_worker();

  QStringList paths_;
  QTreeWidget* tree_ = nullptr;
  QProgressBar* progress_ = nullptr;
  QPushButton* compute_btn_ = nullptr;
  QPushButton* cached_btn_ = nullptr;
  QPushButton* cancel_btn_ = nullptr;
  QThread* thread_ = nullptr;
  QObject* worker_ = nullptr;
};

/// Show checksum dialog for the given paths (compute/refresh via ChecksumStore).
void show_checksum_dialog(QWidget* parent, const std::vector<std::filesystem::path>& paths);
void show_checksum_dialog(QWidget* parent, const QStringList& paths);

} // namespace dirtoo::app

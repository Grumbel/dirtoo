// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QString>
#include <atomic>
#include <cstdint>

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QElapsedTimer;

namespace dirtoo::app {

/// Progress UI for multi-file paste/copy operations.
class TransferDialog : public QDialog {
  Q_OBJECT

public:
  explicit TransferDialog(QWidget* parent = nullptr);

  void set_title_text(const QString& text);
  void set_destination(const QString& path);
  void set_current_file(const QString& path);
  void set_progress(std::uint64_t done, std::uint64_t total);
  void set_item_progress(int current_item, int total_items);
  void append_log(const QString& line);
  void mark_finished(bool cancelled, const QString& error = {});

  [[nodiscard]] bool is_cancelled() const noexcept { return cancelled_.load(); }
  [[nodiscard]] bool close_when_finished() const;
  void reset();

signals:
  void cancel_requested();
  void pause_requested();
  void resume_requested();

private slots:
  void on_cancel();
  void on_pause_toggle();
  void on_tick();

private:
  void update_transferred_label();
  void update_time_label();

  QLabel* title_label_ = nullptr;
  QLabel* dest_label_ = nullptr;
  QLabel* file_label_ = nullptr;
  QLabel* item_label_ = nullptr;
  QLabel* transferred_label_ = nullptr;
  QLabel* time_label_ = nullptr;
  QProgressBar* bar_ = nullptr;
  QPlainTextEdit* log_ = nullptr;
  QCheckBox* close_when_finished_ = nullptr;
  QPushButton* pause_btn_ = nullptr;
  QPushButton* cancel_btn_ = nullptr;
  QPushButton* close_btn_ = nullptr;
  std::atomic<bool> cancelled_{false};
  bool paused_ = false;
  std::uint64_t bytes_done_ = 0;
  std::uint64_t bytes_total_ = 0;
  QElapsedTimer* elapsed_ = nullptr;
  class QTimer* ui_timer_ = nullptr;
};

} // namespace dirtoo::app

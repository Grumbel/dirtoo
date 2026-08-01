// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QString>
#include <atomic>
#include <cstdint>

class QLabel;
class QProgressBar;
class QPushButton;

namespace dirtoo::app {

/// Progress UI for multi-file paste/copy operations.
class TransferDialog : public QDialog {
  Q_OBJECT

public:
  explicit TransferDialog(QWidget* parent = nullptr);

  void set_title_text(const QString& text);
  void set_current_file(const QString& path);
  void set_progress(std::uint64_t done, std::uint64_t total);
  void set_item_progress(int current_item, int total_items);

  [[nodiscard]] bool is_cancelled() const noexcept { return cancelled_.load(); }
  void reset();

signals:
  void cancel_requested();

private slots:
  void on_cancel();

private:
  QLabel* title_label_ = nullptr;
  QLabel* file_label_ = nullptr;
  QLabel* item_label_ = nullptr;
  QProgressBar* bar_ = nullptr;
  QPushButton* cancel_btn_ = nullptr;
  std::atomic<bool> cancelled_{false};
};

} // namespace dirtoo::app

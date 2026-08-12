// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <Qt>

class QLineEdit;
class QWidget;

namespace dirtoo::app {

/// Owns filter + recursive-search row chrome (labels, edits, Help buttons).
/// MainWindow embeds the host widgets from create_*() and connects signals;
/// event filters (history keys, Escape, FocusOut) stay on MainWindow.
class FilterSearchChrome : public QObject {
  Q_OBJECT
public:
  explicit FilterSearchChrome(QObject* parent = nullptr);

  /// Build the bottom filter row. Parent owns the returned widget in the
  /// layout tree; this object keeps non-owning pointers.
  [[nodiscard]] QWidget* create_filter_row(QWidget* parent);

  /// Build the recursive-search row (initially hidden).
  [[nodiscard]] QWidget* create_search_row(QWidget* parent);

  [[nodiscard]] QWidget* filter_row() const { return filter_row_; }
  [[nodiscard]] QLineEdit* filter_edit() const { return filter_edit_; }
  [[nodiscard]] QWidget* search_row() const { return search_row_; }
  [[nodiscard]] QLineEdit* search_edit() const { return search_edit_; }

  [[nodiscard]] QString filter_text() const;
  [[nodiscard]] QString search_text() const;

  void set_filter_visible(bool visible);
  void set_search_visible(bool visible);
  void focus_filter(Qt::FocusReason reason = Qt::OtherFocusReason);
  void focus_search(Qt::FocusReason reason = Qt::OtherFocusReason);
  void clear_filter();
  void set_filter_text(const QString& text);
  void clear_search();

  /// Reset filter-row palette to app Window/Base so it does not inherit a
  /// tinted view Base (parity with update_filter_chrome bar portion).
  void reset_filter_bar_palette();

signals:
  void filter_text_changed(const QString& text);
  void search_submitted();
  void help_requested();

private:
  QWidget* filter_row_ = nullptr;
  QLineEdit* filter_edit_ = nullptr;
  QWidget* search_row_ = nullptr;
  QLineEdit* search_edit_ = nullptr;
};

} // namespace dirtoo::app

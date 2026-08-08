// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"
#include "path_completion_service.hpp"

#include <QLineEdit>
#include <Qt>
#include <QObject>
#include <QString>
#include <QUrl>

class QWidget;

namespace dirtoo::app {

class LocationButtonBar;

/// Owns location bar chrome: breadcrumb bar ↔ line edit + path completion.
/// MainWindow embeds the host widget from create_bar() and connects signals.
class LocationChrome : public QObject {
  Q_OBJECT
public:
  explicit LocationChrome(QObject* parent = nullptr);

  /// Build the Location: label + breadcrumb / line-edit row. Parent owns the
  /// returned widget in the layout tree; this object keeps non-owning pointers.
  [[nodiscard]] QWidget* create_bar(QWidget* parent);

  void setup_completion();
  void shutdown();

  void show_buttons();
  void show_line_edit();
  void focus_line_edit(Qt::FocusReason reason = Qt::OtherFocusReason);

  /// Sync breadcrumb + line-edit text from the current navigation location.
  void set_location(const dirtoo::fs::Location& location);

  [[nodiscard]] PathCompletionService& path_completion() { return path_completion_; }
  [[nodiscard]] QLineEdit* edit() const { return edit_; }
  [[nodiscard]] LocationButtonBar* buttons() const { return buttons_; }
  [[nodiscard]] QWidget* host() const { return host_; }
  [[nodiscard]] bool line_edit_visible() const;

signals:
  void location_activated(const dirtoo::fs::Location& location);
  void location_activated_new_window(const dirtoo::fs::Location& location);
  void path_entered(const QString& text);
  void urls_dropped(const dirtoo::fs::Location& target, const QList<QUrl>& urls,
                    Qt::DropAction action);

private:
  PathCompletionService path_completion_{this};
  QWidget* host_ = nullptr;
  QLineEdit* edit_ = nullptr;
  LocationButtonBar* buttons_ = nullptr;
};

} // namespace dirtoo::app

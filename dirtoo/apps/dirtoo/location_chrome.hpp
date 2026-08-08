// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "path_completion_service.hpp"

#include <QLineEdit>
#include <Qt>
#include <QObject>
#include <QString>
#include <QStringList>

namespace dirtoo::app {

class LocationButtonBar;

/// Location bar chrome: breadcrumb bar ↔ line edit + path completion.
/// Call bind() after the widgets exist, then setup_completion().
class LocationChrome : public QObject {
  Q_OBJECT
public:
  explicit LocationChrome(QObject* parent = nullptr);

  void bind(QLineEdit* edit, LocationButtonBar* buttons);
  void setup_completion();
  void shutdown();

  void show_buttons();
  void show_line_edit();
  void focus_line_edit(Qt::FocusReason reason = Qt::OtherFocusReason);

  void on_text_edited(const QString& text);
  void on_timeout();
  void on_completions_ready(quint64 request_id, const QString& longest,
                            const QStringList& candidates);

  [[nodiscard]] PathCompletionService& path_completion() { return path_completion_; }
  [[nodiscard]] QLineEdit* edit() const { return edit_; }
  [[nodiscard]] LocationButtonBar* buttons() const { return buttons_; }

private:
  PathCompletionService path_completion_{this};
  QLineEdit* edit_ = nullptr;
  LocationButtonBar* buttons_ = nullptr;
};

} // namespace dirtoo::app

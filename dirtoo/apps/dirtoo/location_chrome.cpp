// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "location_chrome.hpp"

#include "location_button_bar.hpp"

namespace dirtoo::app {

LocationChrome::LocationChrome(QObject* parent)
    : QObject(parent)
{
}

void LocationChrome::bind(QLineEdit* edit, LocationButtonBar* buttons)
{
  edit_ = edit;
  buttons_ = buttons;
}

void LocationChrome::setup_completion()
{
  if (edit_ != nullptr) {
    path_completion_.setup(edit_);
  }
}

void LocationChrome::shutdown()
{
  path_completion_.shutdown();
}

void LocationChrome::show_buttons()
{
  if (buttons_ != nullptr) {
    buttons_->show();
  }
  if (edit_ != nullptr) {
    edit_->hide();
  }
}

void LocationChrome::show_line_edit()
{
  if (buttons_ != nullptr) {
    buttons_->hide();
  }
  if (edit_ != nullptr) {
    edit_->show();
  }
}

void LocationChrome::focus_line_edit(Qt::FocusReason reason)
{
  show_line_edit();
  if (edit_ != nullptr) {
    edit_->setFocus(reason);
    edit_->selectAll();
  }
}

void LocationChrome::on_text_edited(const QString& text)
{
  path_completion_.on_text_edited(text);
}

void LocationChrome::on_timeout()
{
  path_completion_.on_timeout();
}

void LocationChrome::on_completions_ready(quint64 request_id, const QString& longest,
                                         const QStringList& candidates)
{
  path_completion_.on_completions_ready(request_id, longest, candidates);
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "location_chrome.hpp"

#include "location_button_bar.hpp"
#include "theme_icons.hpp"

#include <QHBoxLayout>
#include <QToolButton>
#include <QSize>
#include <QLabel>
#include <QVBoxLayout>

namespace dirtoo::app {

LocationChrome::LocationChrome(QObject* parent)
    : QObject(parent)
{
}

QWidget* LocationChrome::create_bar(QWidget* parent)
{
  host_ = new QWidget(parent);
  auto* loc_layout = new QVBoxLayout(host_);
  loc_layout->setContentsMargins(0, 0, 0, 0);
  loc_layout->setSpacing(0);

  auto* breadcrumb_row = new QWidget(host_);
  auto* breadcrumb_layout = new QHBoxLayout(breadcrumb_row);
  breadcrumb_layout->setContentsMargins(4, 2, 4, 2);
  breadcrumb_layout->setSpacing(6);
  auto* loc_label = new QLabel(QStringLiteral("Location:"), breadcrumb_row);
  loc_label->setStyleSheet(QStringLiteral("font-weight: bold;"));
  breadcrumb_layout->addWidget(loc_label);

  buttons_ = new LocationButtonBar(breadcrumb_row);
  breadcrumb_layout->addWidget(buttons_, 1);
  connect(buttons_, &LocationButtonBar::location_activated, this,
          &LocationChrome::location_activated);
  connect(buttons_, &LocationButtonBar::location_activated_new_window, this,
          &LocationChrome::location_activated_new_window);
  connect(buttons_, &LocationButtonBar::edit_requested, this, [this] {
    focus_line_edit(Qt::MouseFocusReason);
  });
  connect(buttons_, &LocationButtonBar::urls_dropped, this, &LocationChrome::urls_dropped);
  connect(buttons_, &LocationButtonBar::query_indicator_activated, this,
          &LocationChrome::query_indicator_activated);

  edit_ = new QLineEdit(breadcrumb_row);
  edit_->setPlaceholderText(QStringLiteral("Location"));
  connect(edit_, &QLineEdit::textEdited, this, [this](const QString& text) {
    path_completion_.on_text_edited(text);
  });
  connect(edit_, &QLineEdit::returnPressed, this, [this] {
    if (edit_ != nullptr) {
      emit path_entered(edit_->text());
    }
  });
  connect(edit_, &QLineEdit::editingFinished, this, [this] {
    if (edit_ != nullptr && !edit_->hasFocus()) {
      show_buttons();
    }
  });
  edit_->hide();
  breadcrumb_layout->addWidget(edit_, 1);

  bookmark_btn_ = new QToolButton(breadcrumb_row);
  bookmark_btn_->setAutoRaise(true);
  bookmark_btn_->setIconSize(QSize(20, 20));
  bookmark_btn_->setFocusPolicy(Qt::NoFocus);
  bookmark_btn_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  connect(bookmark_btn_, &QToolButton::clicked, this, &LocationChrome::bookmark_clicked);
  breadcrumb_layout->addWidget(bookmark_btn_, 0, Qt::AlignVCenter);
  set_bookmarked(false);

  loc_layout->addWidget(breadcrumb_row);
  return host_;
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

void LocationChrome::set_location(const dirtoo::fs::Location& location)
{
  if (edit_ != nullptr) {
    if (location.is_archive() || location.is_tag() || location.is_set()) {
      edit_->setText(QString::fromStdString(location.as_url()));
    } else {
      edit_->setText(QString::fromStdString(location.as_path().string()));
    }
  }
  if (buttons_ != nullptr) {
    buttons_->set_location(location);
  }
  show_buttons();
}

void LocationChrome::clear_query_tip()
{
  if (buttons_ != nullptr) {
    buttons_->clear_query_tip();
  }
}


void LocationChrome::set_bookmarked(bool bookmarked)
{
  bookmarked_ = bookmarked;
  if (bookmark_btn_ == nullptr) {
    return;
  }
  if (bookmarked) {
    bookmark_btn_->setIcon(theme_icon("bookmark-remove", "starred"));
    bookmark_btn_->setToolTip(QStringLiteral("Remove bookmark for this location (Ctrl+D)"));
    bookmark_btn_->setAccessibleName(QStringLiteral("Remove bookmark"));
  } else {
    bookmark_btn_->setIcon(theme_icon("bookmark-new", "non-starred"));
    bookmark_btn_->setToolTip(QStringLiteral("Bookmark this location (Ctrl+D)"));
    bookmark_btn_->setAccessibleName(QStringLiteral("Add bookmark"));
  }
}

bool LocationChrome::line_edit_visible() const
{
  return edit_ != nullptr && edit_->isVisible();
}

} // namespace dirtoo::app

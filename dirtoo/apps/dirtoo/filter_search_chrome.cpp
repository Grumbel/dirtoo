// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filter_search_chrome.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QApplication>

namespace dirtoo::app {

FilterSearchChrome::FilterSearchChrome(QObject* parent)
    : QObject(parent)
{
}

QWidget* FilterSearchChrome::create_filter_row(QWidget* parent)
{
  filter_row_ = new QWidget(parent);
  filter_row_->setAutoFillBackground(true);
  filter_row_->setBackgroundRole(QPalette::Window);
  auto* filter_layout = new QHBoxLayout(filter_row_);
  filter_layout->setContentsMargins(6, 2, 6, 2);
  filter_layout->setSpacing(6);
  auto* filter_label = new QLabel(QStringLiteral("Filter:"), filter_row_);
  filter_edit_ = new QLineEdit(filter_row_);
  filter_edit_->setPlaceholderText(
      QStringLiteral("Filter by name, glob, or expression (e.g. *.png, size:>1M)…"));
  filter_edit_->setClearButtonEnabled(true);
  filter_edit_->setEnabled(true);
  filter_edit_->setVisible(true);
  connect(filter_edit_, &QLineEdit::textChanged, this, &FilterSearchChrome::filter_text_changed);
  filter_label->setBuddy(filter_edit_);
  auto* filter_help_btn = new QPushButton(QStringLiteral("Help"), filter_row_);
  filter_help_btn->setToolTip(QStringLiteral("Filter expression language help"));
  filter_help_btn->setFlat(false);
  connect(filter_help_btn, &QPushButton::clicked, this, &FilterSearchChrome::help_requested);
  filter_layout->addWidget(filter_label);
  filter_layout->addWidget(filter_edit_, 1);
  filter_layout->addWidget(filter_help_btn);
  return filter_row_;
}

QWidget* FilterSearchChrome::create_search_row(QWidget* parent)
{
  search_row_ = new QWidget(parent);
  auto* search_layout = new QHBoxLayout(search_row_);
  search_layout->setContentsMargins(6, 2, 6, 2);
  search_layout->setSpacing(6);
  auto* search_label = new QLabel(QStringLiteral("Search:"), search_row_);
  search_edit_ = new QLineEdit(search_row_);
  search_edit_->setPlaceholderText(
      QStringLiteral("Recursive search (filter expression, Enter to run, Esc to close)…"));
  search_edit_->setVisible(true);
  connect(search_edit_, &QLineEdit::returnPressed, this, &FilterSearchChrome::search_submitted);
  search_label->setBuddy(search_edit_);
  auto* search_help_btn = new QPushButton(QStringLiteral("Help"), search_row_);
  search_help_btn->setToolTip(QStringLiteral("Filter expression language help"));
  connect(search_help_btn, &QPushButton::clicked, this, &FilterSearchChrome::help_requested);
  search_layout->addWidget(search_label);
  search_layout->addWidget(search_edit_, 1);
  search_layout->addWidget(search_help_btn);
  search_row_->setVisible(false);
  return search_row_;
}

QString FilterSearchChrome::filter_text() const
{
  return filter_edit_ != nullptr ? filter_edit_->text() : QString();
}

QString FilterSearchChrome::search_text() const
{
  return search_edit_ != nullptr ? search_edit_->text() : QString();
}

void FilterSearchChrome::set_filter_visible(bool visible)
{
  if (filter_row_ != nullptr) {
    filter_row_->setVisible(visible);
  }
}

void FilterSearchChrome::set_search_visible(bool visible)
{
  if (search_row_ != nullptr) {
    search_row_->setVisible(visible);
  } else if (search_edit_ != nullptr) {
    search_edit_->setVisible(visible);
  }
}

void FilterSearchChrome::focus_filter(Qt::FocusReason reason)
{
  if (filter_edit_ == nullptr) {
    return;
  }
  filter_edit_->setVisible(true);
  filter_edit_->setEnabled(true);
  filter_edit_->setFocus(reason);
  filter_edit_->selectAll();
}

void FilterSearchChrome::focus_search(Qt::FocusReason reason)
{
  if (search_edit_ == nullptr) {
    return;
  }
  search_edit_->setFocus(reason);
  search_edit_->selectAll();
}

void FilterSearchChrome::clear_filter()
{
  if (filter_edit_ != nullptr) {
    filter_edit_->clear();
  }
}

void FilterSearchChrome::clear_search()
{
  if (search_edit_ != nullptr) {
    search_edit_->clear();
  }
}

void FilterSearchChrome::reset_filter_bar_palette()
{
  const QColor app_base = qApp->palette().color(QPalette::Base);
  const QColor app_window = qApp->palette().color(QPalette::Window);
  if (filter_row_ != nullptr) {
    filter_row_->setAutoFillBackground(true);
    QPalette bar_pal = filter_row_->palette();
    bar_pal.setColor(QPalette::Window, app_window);
    bar_pal.setColor(QPalette::Base, app_base);
    filter_row_->setPalette(bar_pal);
    filter_row_->setStyleSheet(QString());
  }
  if (filter_edit_ != nullptr) {
    QPalette edit_pal = filter_edit_->palette();
    edit_pal.setColor(QPalette::Base, app_base);
    edit_pal.setColor(QPalette::Window, app_base);
    filter_edit_->setPalette(edit_pal);
  }
}

} // namespace dirtoo::app

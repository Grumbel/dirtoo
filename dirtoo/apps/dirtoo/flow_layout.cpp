// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flow_layout.hpp"

#include <QWidget>

#include <algorithm>

namespace dirtoo::app {

FlowLayout::FlowLayout(QWidget* parent, int margin, int h_spacing, int v_spacing)
    : QLayout(parent)
    , h_space_(h_spacing)
    , v_space_(v_spacing)
{
  if (margin >= 0) {
    setContentsMargins(margin, margin, margin, margin);
  }
}

FlowLayout::~FlowLayout()
{
  while (QLayoutItem* item = takeAt(0)) {
    delete item;
  }
}

void FlowLayout::addItem(QLayoutItem* item)
{
  items_.push_back(item);
}

int FlowLayout::count() const
{
  return static_cast<int>(items_.size());
}

QLayoutItem* FlowLayout::itemAt(int index) const
{
  if (index < 0 || index >= count()) {
    return nullptr;
  }
  return items_[static_cast<std::size_t>(index)];
}

QLayoutItem* FlowLayout::takeAt(int index)
{
  if (index < 0 || index >= count()) {
    return nullptr;
  }
  QLayoutItem* item = items_[static_cast<std::size_t>(index)];
  items_.erase(items_.begin() + index);
  return item;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
  return {};
}

bool FlowLayout::hasHeightForWidth() const
{
  return true;
}

int FlowLayout::heightForWidth(int width) const
{
  return do_layout(QRect(0, 0, width, 0), true);
}

void FlowLayout::setGeometry(const QRect& rect)
{
  QLayout::setGeometry(rect);
  do_layout(rect, false);
}

QSize FlowLayout::sizeHint() const
{
  return minimumSize();
}

QSize FlowLayout::minimumSize() const
{
  QSize size;
  for (QLayoutItem* item : items_) {
    size = size.expandedTo(item->minimumSize());
  }
  const QMargins m = contentsMargins();
  size += QSize(m.left() + m.right(), m.top() + m.bottom());
  return size;
}

int FlowLayout::smart_spacing(QStyle::PixelMetric pm) const
{
  QObject* parent = this->parent();
  if (parent == nullptr) {
    return -1;
  }
  if (parent->isWidgetType()) {
    auto* pw = static_cast<QWidget*>(parent);
    return pw->style()->pixelMetric(pm, nullptr, pw);
  }
  return static_cast<QLayout*>(parent)->spacing();
}

int FlowLayout::do_layout(const QRect& rect, bool test_only) const
{
  int left, top, right, bottom;
  getContentsMargins(&left, &top, &right, &bottom);
  QRect effective = rect.adjusted(+left, +top, -right, -bottom);
  int x = effective.x();
  int y = effective.y();
  int line_height = 0;

  const int h_space = h_space_ >= 0 ? h_space_ : smart_spacing(QStyle::PM_LayoutHorizontalSpacing);
  const int v_space = v_space_ >= 0 ? v_space_ : smart_spacing(QStyle::PM_LayoutVerticalSpacing);
  const int space_x = h_space >= 0 ? h_space : 4;
  const int space_y = v_space >= 0 ? v_space : 4;

  for (QLayoutItem* item : items_) {
    int next_x = x + item->sizeHint().width() + space_x;
    if (next_x - space_x > effective.right() && line_height > 0) {
      x = effective.x();
      y = y + line_height + space_y;
      next_x = x + item->sizeHint().width() + space_x;
      line_height = 0;
    }
    if (!test_only) {
      item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));
    }
    x = next_x;
    line_height = std::max(line_height, item->sizeHint().height());
  }
  return y + line_height - rect.y() + bottom;
}

} // namespace dirtoo::app

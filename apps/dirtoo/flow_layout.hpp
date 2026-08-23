// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QLayout>
#include <QStyle>
#include <QWidget>

#include <vector>

namespace dirtoo::app {

/// Left-to-right, top-to-bottom wrap layout.
class FlowLayout : public QLayout {
public:
  explicit FlowLayout(QWidget* parent = nullptr, int margin = -1, int h_spacing = -1,
                      int v_spacing = -1);
  ~FlowLayout() override;

  void addItem(QLayoutItem* item) override;
  [[nodiscard]] int count() const override;
  [[nodiscard]] QLayoutItem* itemAt(int index) const override;
  QLayoutItem* takeAt(int index) override;

  [[nodiscard]] Qt::Orientations expandingDirections() const override;
  [[nodiscard]] bool hasHeightForWidth() const override;
  [[nodiscard]] int heightForWidth(int width) const override;

  void setGeometry(const QRect& rect) override;
  [[nodiscard]] QSize sizeHint() const override;
  [[nodiscard]] QSize minimumSize() const override;

private:
  int do_layout(const QRect& rect, bool test_only) const;
  int smart_spacing(QStyle::PixelMetric pm) const;

  std::vector<QLayoutItem*> items_;
  int h_space_ = -1;
  int v_space_ = -1;
};

} // namespace dirtoo::app

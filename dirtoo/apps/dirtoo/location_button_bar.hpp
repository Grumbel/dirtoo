// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <QWidget>

#include <vector>

class QHBoxLayout;
class QPushButton;

namespace dirtoo::app {

/// Breadcrumb-style location control: each path segment is a button that
/// navigates to that ancestor. Empty area click requests the line-edit form.
class LocationButtonBar : public QWidget {
  Q_OBJECT

public:
  explicit LocationButtonBar(QWidget* parent = nullptr);

  void set_location(const fs::Location& location);
  [[nodiscard]] fs::Location location() const { return location_; }

signals:
  void location_activated(const dirtoo::fs::Location& location);
  /// User clicked outside of a segment button (e.g. to edit the path as text).
  void edit_requested();

protected:
  void mousePressEvent(QMouseEvent* event) override;

private:
  void rebuild();
  [[nodiscard]] std::vector<std::pair<QString, fs::Location>> segments_for(const fs::Location& location) const;

  fs::Location location_;
  QHBoxLayout* layout_ = nullptr;
};

} // namespace dirtoo::app

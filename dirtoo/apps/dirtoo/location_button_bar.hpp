// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/fs/location.hpp"

#include <QWidget>

#include <vector>

class QHBoxLayout;
class QMouseEvent;
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
  void edit_requested();
  /// Files dropped onto a breadcrumb segment (target directory location).
  void urls_dropped(const dirtoo::fs::Location& target, const QList<QUrl>& urls,
                    Qt::DropAction action);

protected:
  void mousePressEvent(QMouseEvent* event) override;

private:
  void rebuild();
  [[nodiscard]] std::vector<std::pair<QString, fs::Location>> segments_for(const fs::Location& location) const;

  fs::Location location_;
  QHBoxLayout* layout_ = nullptr;
};

} // namespace dirtoo::app

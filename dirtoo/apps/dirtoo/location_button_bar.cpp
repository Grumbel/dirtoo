// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "location_button_bar.hpp"

#include <algorithm>

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>

namespace dirtoo::app {

LocationButtonBar::LocationButtonBar(QWidget* parent)
    : QWidget(parent)
{
  layout_ = new QHBoxLayout(this);
  layout_->setContentsMargins(0, 0, 0, 0);
  layout_->setSpacing(2);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setCursor(Qt::PointingHandCursor);
}

void LocationButtonBar::set_location(const fs::Location& location)
{
  location_ = location;
  rebuild();
}

void LocationButtonBar::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton) {
    emit edit_requested();
  }
  QWidget::mousePressEvent(event);
}

std::vector<std::pair<QString, fs::Location>>
LocationButtonBar::segments_for(const fs::Location& location) const
{
  std::vector<std::pair<QString, fs::Location>> segs;

  if (location.is_archive()) {
    // Archive file path segments, then internal entry path.
    const auto archive_file = location.as_path();
    std::vector<std::filesystem::path> parts;
    for (std::filesystem::path p = archive_file; !p.empty(); p = p.parent_path()) {
      if (p == p.root_path()) {
        parts.push_back(p);
        break;
      }
      parts.push_back(p);
      if (p.parent_path() == p) {
        break;
      }
    }
    std::reverse(parts.begin(), parts.end());

    for (std::size_t i = 0; i < parts.size(); ++i) {
      const bool is_last_file_seg = (i + 1 == parts.size());
      QString label;
      if (parts[i] == parts[i].root_path()) {
        label = QStringLiteral("/");
      } else if (is_last_file_seg) {
        label = QString::fromStdString(parts[i].filename().string()) + QStringLiteral(" [archive]");
      } else {
        label = QString::fromStdString(parts[i].filename().string());
      }

      if (is_last_file_seg) {
        segs.emplace_back(label, fs::Location::from_archive(archive_file, {}));
      } else {
        segs.emplace_back(label, fs::Location::from_path(parts[i]));
      }
    }

    const auto entry = location.entry_path().lexically_normal();
    if (!entry.empty()) {
      std::filesystem::path acc;
      for (const auto& piece : entry) {
        if (piece == "." || piece.empty()) {
          continue;
        }
        acc /= piece;
        segs.emplace_back(QString::fromStdString(piece.string()),
                          fs::Location::from_archive(archive_file, acc));
      }
    }
    return segs;
  }

  // Normal file location.
  const auto path = location.as_path();
  std::vector<std::filesystem::path> parts;
  for (std::filesystem::path p = path; !p.empty(); p = p.parent_path()) {
    parts.push_back(p);
    if (p == p.root_path() || p.parent_path() == p) {
      break;
    }
  }
  std::reverse(parts.begin(), parts.end());

  for (const auto& p : parts) {
    QString label;
    if (p == p.root_path()) {
      label = QStringLiteral("/");
    } else {
      label = QString::fromStdString(p.filename().string());
    }
    segs.emplace_back(label, fs::Location::from_path(p));
  }
  return segs;
}

void LocationButtonBar::rebuild()
{
  while (QLayoutItem* item = layout_->takeAt(0)) {
    if (QWidget* w = item->widget()) {
      w->deleteLater();
    }
    delete item;
  }

  if (location_.empty()) {
    return;
  }

  const auto segs = segments_for(location_);
  QPushButton* last = nullptr;
  for (const auto& [label, loc] : segs) {
    auto* btn = new QPushButton(label, this);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    btn->setMinimumWidth(4);
    btn->setFocusPolicy(Qt::NoFocus);
    // Style as a breadcrumb chip.
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { padding: 2px 6px; border-radius: 3px; }"
        "QPushButton:hover { background: palette(mid); }"
        "QPushButton:pressed { background: palette(dark); }"));

    const fs::Location target = loc;
    connect(btn, &QPushButton::clicked, this, [this, target] { emit location_activated(target); });
    layout_->addWidget(btn);
    last = btn;
  }

  if (last != nullptr) {
    last->setEnabled(false); // current location — not clickable
    last->setStyleSheet(QStringLiteral(
        "QPushButton { padding: 2px 6px; font-weight: bold; border-radius: 3px; }"));
  }

  layout_->addStretch(1);
}

} // namespace dirtoo::app

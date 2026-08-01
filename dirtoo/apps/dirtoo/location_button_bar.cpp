// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "location_button_bar.hpp"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QIcon>
#include <QHBoxLayout>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QUrl>

#include <algorithm>
#include <functional>

namespace dirtoo::app {

class SegmentButton : public QPushButton {
public:
  SegmentButton(const fs::Location& location, const QString& label, QWidget* parent)
      : QPushButton(label, parent)
      , location_(location)
  {
    // Match dirtoo-py LocationButton: normal QPushButton chrome, not flat
    // text chips with a highlight fill. Current segment uses setDown(true).
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    setMinimumWidth(4);
    setFocusPolicy(Qt::NoFocus);
    setAcceptDrops(true);
    setStyleSheet(QStringLiteral("QPushButton { padding: 3px 4px; }"));
  }

  [[nodiscard]] const fs::Location& location() const { return location_; }

  void set_current(bool current)
  {
    // Python: button.setDown(True) on the current ancestry segment only.
    setDown(current);
  }

  std::function<void(const fs::Location&, const QList<QUrl>&, Qt::DropAction)> on_drop;
  std::function<void(const fs::Location&)> on_middle_click;

protected:
  void mouseReleaseEvent(QMouseEvent* event) override
  {
    if (event->button() == Qt::MiddleButton && on_middle_click) {
      on_middle_click(location_);
      event->accept();
      return;
    }
    QPushButton::mouseReleaseEvent(event);
  }

  void dragEnterEvent(QDragEnterEvent* event) override
  {
    if (event->mimeData() != nullptr && event->mimeData()->hasUrls()) {
      event->acceptProposedAction();
    } else {
      event->ignore();
    }
  }

  void dropEvent(QDropEvent* event) override
  {
    if (event->mimeData() == nullptr || !event->mimeData()->hasUrls()) {
      event->ignore();
      return;
    }
    if (on_drop) {
      on_drop(location_, event->mimeData()->urls(), event->proposedAction());
    }
    event->acceptProposedAction();
  }

private:
  fs::Location location_;
};

LocationButtonBar::LocationButtonBar(QWidget* parent)
    : QWidget(parent)
{
  layout_ = new QHBoxLayout(this);
  layout_->setContentsMargins(0, 0, 0, 0);
  layout_->setSpacing(0);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setCursor(Qt::PointingHandCursor);
  setMinimumHeight(28);
}

void LocationButtonBar::wire_button(SegmentButton* btn)
{
  const fs::Location target = btn->location();
  connect(btn, &QPushButton::clicked, this, [this, target] { emit location_activated(target); });
  btn->on_drop = [this](const fs::Location& dest, const QList<QUrl>& urls, Qt::DropAction action) {
    emit urls_dropped(dest, urls, action);
  };
  btn->on_middle_click = [this](const fs::Location& dest) {
    emit location_activated_new_window(dest);
  };
}

int LocationButtonBar::index_of_location(const fs::Location& location) const
{
  for (int i = 0; i < static_cast<int>(buttons_.size()); ++i) {
    if (buttons_[static_cast<std::size_t>(i)] != nullptr
        && buttons_[static_cast<std::size_t>(i)]->location() == location) {
      return i;
    }
  }
  return -1;
}

void LocationButtonBar::update_current_highlight()
{
  for (SegmentButton* btn : buttons_) {
    if (btn == nullptr) {
      continue;
    }
    btn->set_current(btn->location() == location_);
  }
}

void LocationButtonBar::set_location(const fs::Location& location)
{
  location_ = location;

  // If this location is already on the trail (navigating to an ancestor), keep
  // deeper segment buttons so the user can jump forward again quickly.
  if (index_of_location(location) >= 0) {
    update_current_highlight();
    return;
  }

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
    const auto archive_file = location.as_path();
    std::vector<std::filesystem::path> parts;
    for (std::filesystem::path p = archive_file; !p.empty(); p = p.parent_path()) {
      parts.push_back(p);
      if (p == p.root_path() || p.parent_path() == p) {
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
  buttons_.clear();

  if (location_.empty()) {
    return;
  }

  const auto segs = segments_for(location_);
  for (std::size_t i = 0; i < segs.size(); ++i) {
    const auto& [label, loc] = segs[i];
    // Root segment: hard-disk icon + empty text (dirtoo-py).
    SegmentButton* btn = nullptr;
    if (label == QLatin1String("/") || label.isEmpty()) {
      btn = new SegmentButton(loc, QString{}, this);
      const QIcon disk = QIcon::fromTheme(QStringLiteral("drive-harddisk"),
                                          QIcon::fromTheme(QStringLiteral("drive-harddisk-solid")));
      if (!disk.isNull()) {
        btn->setIcon(disk);
        btn->setIconSize(QSize(16, 16));
      } else {
        btn->setText(QStringLiteral("/"));
      }
    } else {
      btn = new SegmentButton(loc, label, this);
    }
    const bool is_current = (i + 1 == segs.size());
    btn->set_current(is_current);
    wire_button(btn);
    layout_->addWidget(btn);
    buttons_.push_back(btn);
  }

  layout_->addStretch(1);
}

} // namespace dirtoo::app

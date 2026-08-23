// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "location_button_bar.hpp"
#include "theme_icons.hpp"

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
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    setMinimumWidth(4);
    setFocusPolicy(Qt::NoFocus);
    setAcceptDrops(true);
    setStyleSheet(QStringLiteral("QPushButton { padding: 3px 4px; }"));
  }

  [[nodiscard]] const fs::Location& location() const { return location_; }

  void set_current(bool current) { setDown(current); }

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

namespace {

[[nodiscard]] bool same_filesystem_path(const fs::Location& a, const fs::Location& b)
{
  if (a.is_archive() != b.is_archive() || a.is_tag() != b.is_tag() || a.is_set() != b.is_set()) {
    return false;
  }
  if (a.is_tag() || a.is_set()) {
    return a.as_url() == b.as_url();
  }
  if (a.is_archive()) {
    return a.as_path() == b.as_path() && a.entry_path() == b.entry_path();
  }
  return a.as_path() == b.as_path();
}

/// True when \p ancestor is the same place as \p tip, or a path parent of it
/// (file path / archive entry). Used to keep the filter chip after go-up.
[[nodiscard]] bool is_path_ancestor_or_same(const fs::Location& ancestor, const fs::Location& tip)
{
  if (ancestor.is_tag() || ancestor.is_set() || tip.is_tag() || tip.is_set()) {
    return false;
  }
  if (ancestor.is_archive() != tip.is_archive()) {
    return false;
  }
  if (ancestor.is_archive()) {
    if (ancestor.as_path() != tip.as_path()) {
      return false;
    }
    const auto a = ancestor.entry_path().lexically_normal().generic_string();
    const auto t = tip.entry_path().lexically_normal().generic_string();
    if (a == t) {
      return true;
    }
    if (a.empty()) {
      return true; // archive root is ancestor of any entry
    }
    return t.size() > a.size() && t.compare(0, a.size(), a) == 0
           && (t[a.size()] == '/');
  }
  const auto a = ancestor.as_path().lexically_normal().generic_string();
  const auto t = tip.as_path().lexically_normal().generic_string();
  if (a == t) {
    return true;
  }
  if (a == "/" ) {
    return t.starts_with('/');
  }
  return t.size() > a.size() && t.compare(0, a.size(), a) == 0
         && (t[a.size()] == '/');
}

[[nodiscard]] QString path_tooltip_for(const fs::Location& loc)
{
  if (loc.is_archive()) {
    return QStringLiteral("Go to archive %1").arg(QString::fromStdString(loc.as_url()));
  }
  if (loc.is_tag() || loc.is_set()) {
    return QStringLiteral("Go to %1").arg(QString::fromStdString(loc.as_url()));
  }
  return QStringLiteral("Go to %1").arg(QString::fromStdString(loc.as_path().string()));
}

} // namespace

LocationButtonBar::LocationButtonBar(QWidget* parent)
    : QWidget(parent)
{
  layout_ = new QHBoxLayout(this);
  layout_->setContentsMargins(0, 0, 0, 0);
  layout_->setSpacing(0);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setCursor(Qt::PointingHandCursor);
}

void LocationButtonBar::wire_button(SegmentButton* btn)
{
  connect(btn, &QPushButton::clicked, this, [this, btn] {
    emit location_activated(btn->location());
  });
  btn->on_drop = [this](const fs::Location& dest, const QList<QUrl>& urls, Qt::DropAction action) {
    emit urls_dropped(dest, urls, action);
  };
  btn->on_middle_click = [this](const fs::Location& dest) {
    emit location_activated_new_window(dest);
  };
}

int LocationButtonBar::index_of_path(const fs::Location& location) const
{
  for (int i = 0; i < static_cast<int>(buttons_.size()); ++i) {
    if (buttons_[static_cast<std::size_t>(i)] != nullptr
        && same_filesystem_path(buttons_[static_cast<std::size_t>(i)]->location(), location)) {
      return i;
    }
  }
  return -1;
}

void LocationButtonBar::update_current_highlight()
{
  const bool query_is_tip =
      !location_.filter_query().empty() || !location_.search_query().empty();
  for (SegmentButton* btn : buttons_) {
    if (btn == nullptr) {
      continue;
    }
    // With an active filter/search the trailing query chip is the breadcrumb tip;
    // do not mark the directory segment as current.
    btn->set_current(!query_is_tip && same_filesystem_path(btn->location(), location_));
  }
  if (query_btn_ != nullptr) {
    query_btn_->setDown(query_is_tip);
  }
}

void LocationButtonBar::update_query_tip(const fs::Location& location)
{
  const bool has_query =
      !location.filter_query().empty() || !location.search_query().empty();
  if (has_query) {
    query_tip_ = location;
    return;
  }
  // No query on the incoming location: keep the tip only while browsing its
  // path ancestors (same as deeper breadcrumb segments after go-up). Leaving
  // into an unrelated or deeper path drops the tip.
  if (query_tip_.empty()) {
    return;
  }
  const bool tip_has_query =
      !query_tip_.filter_query().empty() || !query_tip_.search_query().empty();
  if (!tip_has_query) {
    query_tip_ = {};
    return;
  }
  if (is_path_ancestor_or_same(location, query_tip_)) {
    return; // still on the tip path or above it
  }
  query_tip_ = {};
}

void LocationButtonBar::clear_query_tip()
{
  query_tip_ = {};
  if (query_btn_ != nullptr) {
    layout_->removeWidget(query_btn_);
    query_btn_->deleteLater();
    query_btn_ = nullptr;
  }
}

void LocationButtonBar::set_location(const fs::Location& location)
{
  location_ = location;
  update_query_tip(location);

  if (index_of_path(location) >= 0) {
    update_current_highlight();
    sync_query_indicator();
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

void LocationButtonBar::sync_query_indicator()
{
  // Prefer the live location query; otherwise the remembered tip after go-up.
  const fs::Location& src =
      (!location_.filter_query().empty() || !location_.search_query().empty())
          ? location_
          : query_tip_;
  const bool has_filter = !src.filter_query().empty();
  const bool has_search = !src.search_query().empty();
  const bool need = has_filter || has_search;
  const bool query_is_current =
      !location_.filter_query().empty() || !location_.search_query().empty();

  if (!need) {
    if (query_btn_ != nullptr) {
      layout_->removeWidget(query_btn_);
      query_btn_->deleteLater();
      query_btn_ = nullptr;
    }
    return;
  }

  if (query_btn_ == nullptr) {
    query_btn_ = new QPushButton(this);
    query_btn_->setCursor(Qt::PointingHandCursor);
    query_btn_->setFocusPolicy(Qt::NoFocus);
    query_btn_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    query_btn_->setStyleSheet(QStringLiteral("QPushButton { padding: 3px 4px; }"));
    query_btn_->setIconSize(QSize(16, 16));
    connect(query_btn_, &QPushButton::clicked, this, [this] {
      // Activate the tip (may be query_tip_ after go-up).
      const fs::Location go =
          (!location_.filter_query().empty() || !location_.search_query().empty())
              ? location_
              : query_tip_;
      emit location_activated(go);
      emit query_indicator_activated();
    });
    int stretch_at = layout_->count() - 1;
    if (stretch_at < 0) {
      stretch_at = 0;
    }
    layout_->insertWidget(stretch_at, query_btn_);
  }

  QString label;
  if (has_filter) {
    label = QString::fromStdString(src.filter_query());
  }
  if (has_search) {
    const QString s = QString::fromStdString(src.search_query());
    label = label.isEmpty() ? s : (label + QStringLiteral(" · ") + s);
  }
  constexpr int kMaxLabelChars = 32;
  QString shown = label;
  if (shown.size() > kMaxLabelChars) {
    shown = shown.left(kMaxLabelChars - 1) + QChar(0x2026);
  }
  query_btn_->setText(shown);

  const QIcon icon = theme_icon("view-filter", "edit-find");
  if (!icon.isNull()) {
    query_btn_->setIcon(icon);
  }

  QStringList tip_parts;
  if (has_filter) {
    tip_parts << QStringLiteral("Filter: %1")
                     .arg(QString::fromStdString(src.filter_query()));
  }
  if (has_search) {
    tip_parts << QStringLiteral("Search: %1")
                     .arg(QString::fromStdString(src.search_query()));
  }
  if (query_is_current) {
    tip_parts << QStringLiteral("Click to show filter bar");
  } else {
    tip_parts << QStringLiteral("Click to restore this filter");
  }
  query_btn_->setToolTip(tip_parts.join(QStringLiteral("\n")));
  query_btn_->setAccessibleName(QStringLiteral("Active filter or search: %1").arg(label));
  query_btn_->setDown(query_is_current);
  query_btn_->show();
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
  query_btn_ = nullptr;

  if (location_.empty()) {
    return;
  }

  const auto segs = segments_for(location_);
  for (std::size_t i = 0; i < segs.size(); ++i) {
    const auto& [label, loc] = segs[i];
    SegmentButton* btn = nullptr;
    // Only the real filesystem root uses the hard-disk icon.
    if (label == QLatin1String("/")) {
      btn = new SegmentButton(loc, QString{}, this);
      const QIcon disk = QIcon::fromTheme(QStringLiteral("drive-harddisk"),
                                          QIcon::fromTheme(QStringLiteral("drive-harddisk-solid")));
      if (!disk.isNull()) {
        btn->setIcon(disk);
        btn->setIconSize(QSize(16, 16));
      } else {
        btn->setText(QStringLiteral("/"));
      }
      btn->setToolTip(QStringLiteral("Go to filesystem root (/)"));
      btn->setAccessibleName(QStringLiteral("Filesystem root"));
    } else if (label.isEmpty()) {
      // Trailing-slash empty components used to get a second disk icon at the end.
      continue;
    } else {
      btn = new SegmentButton(loc, label, this);
      if (loc.is_archive() && loc.entry_path().empty()) {
        const QIcon pkg = QIcon::fromTheme(
            QStringLiteral("package-x-generic"),
            QIcon::fromTheme(QStringLiteral("application-x-archive"),
                             QIcon::fromTheme(QStringLiteral("folder-tar"))));
        if (!pkg.isNull()) {
          btn->setIcon(pkg);
          btn->setIconSize(QSize(16, 16));
        }
        btn->setToolTip(QStringLiteral("Archive: %1").arg(label));
      } else {
        btn->setToolTip(path_tooltip_for(loc));
      }
      btn->setAccessibleName(label);
    }
    const bool query_is_tip =
        !location_.filter_query().empty() || !location_.search_query().empty();
    btn->set_current(!query_is_tip && same_filesystem_path(loc, location_));
    wire_button(btn);
    layout_->addWidget(btn);
    buttons_.push_back(btn);
  }

  layout_->addStretch(1);
  sync_query_indicator();
}

} // namespace dirtoo::app

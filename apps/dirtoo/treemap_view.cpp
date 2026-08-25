// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "treemap_view.hpp"
#include "file_list_model.hpp"

#include "size_format.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QToolTip>

#include <cmath>

namespace dirtoo::app {
namespace {

QColor color_for_path(const QString& path)
{
  const uint h = qHash(path);
  return QColor::fromHsv(static_cast<int>(h % 360), 90 + static_cast<int>((h >> 8) % 70),
                         150 + static_cast<int>((h >> 16) % 70));
}

} // namespace

TreemapView::TreemapView(QWidget* parent)
    : QWidget(parent)
{
  setMouseTracking(true);
  setMinimumSize(120, 80);
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
}

void TreemapView::set_root(std::shared_ptr<const dirtoo::tree::FsTreeNode> root)
{
  root_ = std::move(root);
  hover_index_ = -1;
  rebuild_layout();
  update();
}

void TreemapView::clear()
{
  root_.reset();
  cells_.clear();
  hover_index_ = -1;
  update();
}

void TreemapView::set_thumbnail_model(FileListModel* model)
{
  thumb_model_ = model;
  update();
}

void TreemapView::rebuild_layout()
{
  cells_.clear();
  if (!root_ || width() < 4 || height() < 4) {
    return;
  }

  std::vector<dirtoo::tree::TreemapInput> inputs;
  std::vector<std::shared_ptr<const dirtoo::tree::FsTreeNode>> nodes;
  inputs.reserve(root_->children().size());
  nodes.reserve(root_->children().size());

  for (const auto& ch : root_->children()) {
    if (!ch) {
      continue;
    }
    const double w = static_cast<double>(ch->total_size());
    if (w <= 0) {
      continue;
    }
    inputs.push_back({ch->path().string(), w});
    nodes.push_back(ch);
  }

  const auto laid = dirtoo::tree::layout_squarified(
      inputs, dirtoo::tree::TreemapRect{0, 0, static_cast<double>(width()),
                                        static_cast<double>(height())});

  // Match cells back by id (path string).
  cells_.reserve(laid.size());
  for (const auto& cell : laid) {
    Cell c;
    c.layout = cell;
    for (const auto& n : nodes) {
      if (n && n->path().string() == cell.id) {
        c.node = n;
        break;
      }
    }
    cells_.push_back(std::move(c));
  }
}

int TreemapView::cell_at(const QPoint& pos) const
{
  for (int i = 0; i < static_cast<int>(cells_.size()); ++i) {
    const auto& r = cells_[static_cast<std::size_t>(i)].layout.rect;
    if (pos.x() >= r.x && pos.x() < r.x + r.w && pos.y() >= r.y && pos.y() < r.y + r.h) {
      return i;
    }
  }
  return -1;
}

void TreemapView::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);
  QPainter p(this);
  p.fillRect(rect(), palette().color(QPalette::Base));

  if (!root_) {
    p.setPen(palette().color(QPalette::PlaceholderText));
    p.drawText(rect(), Qt::AlignCenter,
               QStringLiteral("No folder size data\nTools → Compute Folder Sizes"));
    return;
  }
  if (cells_.empty()) {
    p.setPen(palette().color(QPalette::PlaceholderText));
    p.drawText(rect(), Qt::AlignCenter, QStringLiteral("Empty or zero-size children"));
    return;
  }

  const QFontMetrics fm(font());
  for (int i = 0; i < static_cast<int>(cells_.size()); ++i) {
    const auto& cell = cells_[static_cast<std::size_t>(i)];
    const auto& r = cell.layout.rect;
    QRectF rf(r.x, r.y, r.w, r.h);
    if (rf.width() < 1 || rf.height() < 1) {
      continue;
    }

    QString path;
    if (cell.node) {
      path = QString::fromStdString(cell.node->path().string());
    } else {
      path = QString::fromStdString(cell.layout.id);
    }
    QColor fill = color_for_path(path);
    if (i == hover_index_) {
      fill = fill.lighter(120);
    }
    p.fillRect(rf, fill);

    // Prefer a cached thumbnail when the cell is large enough.
    if (thumb_model_ != nullptr && rf.width() >= 48 && rf.height() >= 48) {
      const QIcon icon = thumb_model_->thumbnail_icon(path);
      if (!icon.isNull()) {
        const QPixmap pm = icon.pixmap(QSize(static_cast<int>(rf.width()), static_cast<int>(rf.height())));
        if (!pm.isNull()) {
          const QSize target(static_cast<int>(rf.width()), static_cast<int>(rf.height()));
          const QPixmap scaled = pm.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
          const int ox = static_cast<int>((scaled.width() - target.width()) / 2);
          const int oy = static_cast<int>((scaled.height() - target.height()) / 2);
          p.drawPixmap(rf.toRect(), scaled, QRect(ox, oy, target.width(), target.height()));
          // Dim overlay so label stays readable.
          p.fillRect(rf, QColor(0, 0, 0, 60));
        }
      }
    }

    p.setPen(QPen(palette().color(QPalette::Window), 1));
    p.drawRect(rf.adjusted(0, 0, -0.5, -0.5));

    if (rf.width() >= 40 && rf.height() >= 18 && cell.node) {
      const QString name = QString::fromStdString(cell.node->name());
      const QString size = format_byte_size(cell.node->total_size());
      const QString label = name + QLatin1Char('\n') + size;
      p.setPen(QColor(255, 255, 255));
      // Shadow for contrast on bright thumbs.
      p.setPen(QColor(0, 0, 0, 180));
      const QRectF text_r = rf.adjusted(3, 2, -3, -2);
      p.drawText(text_r.translated(1, 1), Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, label);
      p.setPen(QColor(255, 255, 255));
      p.drawText(text_r, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, label);
    }
  }
}

void TreemapView::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  rebuild_layout();
}

void TreemapView::mouseMoveEvent(QMouseEvent* event)
{
  const int idx = cell_at(event->pos());
  if (idx != hover_index_) {
    hover_index_ = idx;
    update();
  }
  if (idx >= 0 && cells_[static_cast<std::size_t>(idx)].node) {
    const auto& n = cells_[static_cast<std::size_t>(idx)].node;
    const QString path = QString::fromStdString(n->path().string());
    emit path_hovered(path, n->total_size());
    setToolTip(path + QLatin1Char('\n') + format_byte_size(n->total_size()));
  } else {
    setToolTip({});
  }
  QWidget::mouseMoveEvent(event);
}

void TreemapView::leaveEvent(QEvent* event)
{
  hover_index_ = -1;
  update();
  QWidget::leaveEvent(event);
}

void TreemapView::mouseDoubleClickEvent(QMouseEvent* event)
{
  const int idx = cell_at(event->pos());
  if (idx >= 0 && cells_[static_cast<std::size_t>(idx)].node) {
    const auto& n = cells_[static_cast<std::size_t>(idx)].node;
    emit path_activated(QString::fromStdString(n->path().string()));
  }
  QWidget::mouseDoubleClickEvent(event);
}

} // namespace dirtoo::app

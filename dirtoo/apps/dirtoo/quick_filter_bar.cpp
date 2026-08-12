// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "quick_filter_bar.hpp"

#include "dirtoo/hash/checksum_store.hpp"
#include "dirtoo/tags/tag_store.hpp"

#include <QHBoxLayout>
#include <QMimeDatabase>
#include <QScrollArea>
#include <QSet>
#include <QToolButton>
#include <QFrame>

#include <algorithm>
#include <filesystem>

namespace dirtoo::app {
namespace {

QString category_for_item(const dirtoo::fs::FileInfo& fi, QMimeDatabase& db)
{
  if (fi.is_directory()) {
    return QStringLiteral("directory");
  }
  if (fi.location().is_archive() && fi.location().entry_path().empty()) {
    return QStringLiteral("archive");
  }
  const QString name = QString::fromStdString(fi.basename());
  const QMimeType mt = db.mimeTypeForFile(name, QMimeDatabase::MatchExtension);
  if (!mt.isValid()) {
    return {};
  }
  const QString m = mt.name();
  if (m.startsWith(QLatin1String("image/"))) {
    return QStringLiteral("image");
  }
  if (m.startsWith(QLatin1String("video/"))) {
    return QStringLiteral("video");
  }
  if (m.startsWith(QLatin1String("audio/"))) {
    return QStringLiteral("audio");
  }
  if (m == QLatin1String("application/pdf") || m.contains(QLatin1String("document"))
      || m.startsWith(QLatin1String("text/"))) {
    return QStringLiteral("document");
  }
  if (m.contains(QLatin1String("zip")) || m.contains(QLatin1String("compressed"))
      || m.contains(QLatin1String("archive")) || m.contains(QLatin1String("tar"))) {
    return QStringLiteral("archive");
  }
  return {};
}

QString label_for_type(const QString& cat)
{
  if (cat == QLatin1String("image")) {
    return QStringLiteral("Images");
  }
  if (cat == QLatin1String("video")) {
    return QStringLiteral("Videos");
  }
  if (cat == QLatin1String("audio")) {
    return QStringLiteral("Audio");
  }
  if (cat == QLatin1String("directory")) {
    return QStringLiteral("Folders");
  }
  if (cat == QLatin1String("archive")) {
    return QStringLiteral("Archives");
  }
  if (cat == QLatin1String("document")) {
    return QStringLiteral("Documents");
  }
  return cat;
}

} // namespace

QuickFilterBar::QuickFilterBar(QWidget* parent)
    : QWidget(parent)
{
  auto* outer = new QHBoxLayout(this);
  outer->setContentsMargins(4, 2, 4, 2);
  outer->setSpacing(4);

  scroll_ = new QScrollArea(this);
  scroll_->setWidgetResizable(true);
  scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll_->setFrameShape(QFrame::NoFrame);
  scroll_->setFixedHeight(36);

  strip_ = new QWidget(scroll_);
  strip_layout_ = new QHBoxLayout(strip_);
  strip_layout_->setContentsMargins(0, 0, 0, 0);
  strip_layout_->setSpacing(4);
  strip_layout_->addStretch(1);
  scroll_->setWidget(strip_);
  outer->addWidget(scroll_, 1);

  pin_btn_ = new QToolButton(this);
  pin_btn_->setText(QStringLiteral("Pin filter"));
  pin_btn_->setToolTip(QStringLiteral("Pin the current filter expression as a QuickFilter button"));
  pin_btn_->setAutoRaise(true);
  connect(pin_btn_, &QToolButton::clicked, this, &QuickFilterBar::pin_current_requested);
  outer->addWidget(pin_btn_);

  setVisible(true);
}

void QuickFilterBar::rebuild_from_items(const std::vector<dirtoo::fs::FileInfo>& items)
{
  auto_chips_.clear();
  QMimeDatabase db;
  QSet<QString> types;
  QSet<QString> tags_seen;

  dirtoo::hash::ChecksumStore checksums;
  dirtoo::tags::TagStore tag_store;
  std::string err;
  const bool tags_ok = checksums.open(dirtoo::hash::ChecksumStore::default_path(), &err)
                       && tag_store.open(dirtoo::tags::TagStore::default_path(), &err);

  constexpr std::size_t kMaxScan = 2000;
  const std::size_t n = std::min(items.size(), kMaxScan);
  for (std::size_t i = 0; i < n; ++i) {
    const auto& fi = items[i];
    const QString cat = category_for_item(fi, db);
    if (!cat.isEmpty()) {
      types.insert(cat);
    }
    if (!tags_ok || fi.is_directory() || fi.path().empty()) {
      continue;
    }
    // Only use cached checksums — never hash on the GUI thread.
    std::string key;
    if (fi.location().is_archive()) {
      key = fi.location().as_url();
    } else {
      std::error_code ec;
      const auto abs = std::filesystem::absolute(fi.path(), ec);
      key = ec ? fi.path().string() : abs.lexically_normal().string();
    }
    if (auto dig = checksums.get(key)) {
      for (const auto& t : tag_store.tags_for_sha256(dig->sha256_hex)) {
        tags_seen.insert(QString::fromStdString(t));
        if (tags_seen.size() >= 24) {
          break;
        }
      }
    }
    if (tags_seen.size() >= 24) {
      break;
    }
  }

  // Stable order for type chips.
  const QStringList type_order = {QStringLiteral("directory"), QStringLiteral("image"),
                                  QStringLiteral("video"),     QStringLiteral("audio"),
                                  QStringLiteral("document"),  QStringLiteral("archive")};
  for (const QString& cat : type_order) {
    if (!types.contains(cat)) {
      continue;
    }
    Chip c;
    c.label = label_for_type(cat);
    c.expression = QStringLiteral("type:%1").arg(cat);
    auto_chips_.push_back(std::move(c));
  }

  QStringList tag_list = tags_seen.values();
  std::sort(tag_list.begin(), tag_list.end(),
            [](const QString& a, const QString& b) { return a.toLower() < b.toLower(); });
  for (const QString& t : tag_list) {
    Chip c;
    c.label = t;
    c.expression = QStringLiteral("tag:%1").arg(t);
    auto_chips_.push_back(std::move(c));
  }

  rebuild_buttons();
}

void QuickFilterBar::set_active_expression(const QString& expr)
{
  active_ = expr.trimmed();
  rebuild_buttons();
}

void QuickFilterBar::pin_expression(const QString& expr, const QString& label)
{
  const QString e = expr.trimmed();
  if (e.isEmpty()) {
    return;
  }
  if (pinned_.contains(e)) {
    return;
  }
  pinned_.push_back(e);
  // Store display override in auto list as pinned-only via rebuild
  rebuild_buttons();
  Q_UNUSED(label);
}

void QuickFilterBar::clear_auto_buttons()
{
  if (strip_layout_ == nullptr) {
    return;
  }
  while (QLayoutItem* item = strip_layout_->takeAt(0)) {
    if (item->widget() != nullptr) {
      item->widget()->deleteLater();
    }
    delete item;
  }
}

QToolButton* QuickFilterBar::make_chip(const QString& label, const QString& expression, bool pinned)
{
  auto* btn = new QToolButton(strip_);
  btn->setText(label);
  btn->setCheckable(true);
  btn->setAutoRaise(true);
  btn->setToolTip(expression);
  btn->setChecked(active_ == expression);
  if (pinned) {
    btn->setStyleSheet(QStringLiteral("QToolButton { font-weight: 600; }"));
  }
  connect(btn, &QToolButton::clicked, this, [this, expression, btn](bool checked) {
    if (checked) {
      emit filter_requested(expression);
    } else if (active_ == expression) {
      emit filter_requested(QString());
    }
  });
  // Right-click pinned chip to unpin
  if (pinned) {
    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(btn, &QWidget::customContextMenuRequested, this, [this, expression](const QPoint&) {
      pinned_.removeAll(expression);
      if (active_ == expression) {
        emit filter_requested(QString());
      }
      rebuild_buttons();
    });
  }
  return btn;
}

void QuickFilterBar::rebuild_buttons()
{
  clear_auto_buttons();

  for (const auto& c : auto_chips_) {
    // Skip auto chips that duplicate a pin
    if (pinned_.contains(c.expression)) {
      continue;
    }
    strip_layout_->addWidget(make_chip(c.label, c.expression, false));
  }
  for (const QString& e : pinned_) {
    QString lab = e;
    if (lab.size() > 28) {
      lab = lab.left(25) + QStringLiteral("…");
    }
    strip_layout_->addWidget(make_chip(lab, e, true));
  }
  strip_layout_->addStretch(1);
}

} // namespace dirtoo::app

// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "quick_filter_bar.hpp"
#include "flow_layout.hpp"
#include <QList>
#include <QHash>
#include "set_paint.hpp"
#include "set_membership.hpp"
#include "dirtoo/sets/file_set_store.hpp"
#include "hash_service.hpp"

#include "dirtoo/tags/tag_store.hpp"

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QColorDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QSizePolicy>
#include <QResizeEvent>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QToolButton>
#include <QStyle>
#include <QFrame>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QDialogButtonBox>
#include <QVBoxLayout>

#include <algorithm>
#include <unordered_set>
#include <filesystem>

namespace dirtoo::app {
namespace {

QString normalize_dir_path(QString path)
{
  path = path.trimmed();
  if (path.isEmpty()) {
    return path;
  }
  // Strip file:// and trailing slash (except root).
  if (path.startsWith(QLatin1String("file://"))) {
    path = path.mid(7);
  }
  std::error_code ec;
  const auto abs = std::filesystem::absolute(path.toStdString(), ec);
  QString out = QString::fromStdString(ec ? path.toStdString() : abs.lexically_normal().string());
  while (out.size() > 1 && (out.endsWith(QLatin1Char('/')) || out.endsWith(QLatin1Char('\\')))) {
    out.chop(1);
  }
  return out;
}

QStringList normalize_dir_list(const QStringList& dirs)
{
  QStringList out;
  for (const QString& d : dirs) {
    for (const QString& part : d.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
      const QString n = normalize_dir_path(part);
      if (!n.isEmpty() && !out.contains(n)) {
        out.push_back(n);
      }
    }
  }
  return out;
}

bool path_equals(const QString& a, const QString& b)
{
  return normalize_dir_path(a) == normalize_dir_path(b);
}

bool path_is_under(const QString& child, const QString& parent)
{
  const QString c = normalize_dir_path(child);
  const QString p = normalize_dir_path(parent);
  if (c.isEmpty() || p.isEmpty()) {
    return false;
  }
  if (c == p) {
    return true;
  }
  const QString prefix = p.endsWith(QLatin1Char('/')) ? p : (p + QLatin1Char('/'));
  return c.startsWith(prefix);
}

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

QString scope_to_string(QuickFilterScope s)
{
  switch (s) {
  case QuickFilterScope::Directory:
    return QStringLiteral("directory");
  case QuickFilterScope::Subtree:
    return QStringLiteral("subtree");
  case QuickFilterScope::Everywhere:
  default:
    return QStringLiteral("everywhere");
  }
}

QuickFilterScope scope_from_string(const QString& s)
{
  if (s == QLatin1String("directory")) {
    return QuickFilterScope::Directory;
  }
  if (s == QLatin1String("subtree")) {
    return QuickFilterScope::Subtree;
  }
  return QuickFilterScope::Everywhere;
}

QString pins_settings_path()
{
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/dirtoo");
  return dir + QStringLiteral("/quick_filters.ini");
}

} // namespace

QuickFilterBar::QuickFilterBar(QWidget* parent)
    : QWidget(parent)
{
  auto* outer = new QHBoxLayout(this);
  outer->setContentsMargins(4, 2, 4, 2);
  outer->setSpacing(4);

  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  strip_ = new QWidget(this);
  strip_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  strip_layout_ = new FlowLayout(strip_, /*margin=*/0, /*h=*/4, /*v=*/4);
  outer->addWidget(strip_, 1);

  pin_btn_ = new QToolButton(this);
  pin_btn_->setText(QStringLiteral("Pin filter"));
  pin_btn_->setToolTip(QStringLiteral("Pin the current filter expression as a QuickFilter button"));
  pin_btn_->setAutoRaise(true);
  connect(pin_btn_, &QToolButton::clicked, this, &QuickFilterBar::pin_current_requested);
  outer->addWidget(pin_btn_);

  load_pins();
}

void QuickFilterBar::set_current_directory(const QString& path)
{
  const QString n = normalize_dir_path(path);
  if (n == current_directory_) {
    return;
  }
  current_directory_ = n;
  rebuild_buttons();
}

void QuickFilterBar::rebuild_from_items(const std::vector<dirtoo::fs::FileInfo>& items)
{
  auto_chips_.clear();
  QMimeDatabase db;
  QSet<QString> types;
  QSet<QString> tags_seen;

  auto& hashes = HashService::instance();
  dirtoo::tags::TagStore tag_store;
  std::string err;
  const bool tags_ok = hashes.ensure_open(&err)
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
    std::string key;
    if (fi.location().is_archive()) {
      key = fi.location().as_url();
    } else {
      std::error_code ec;
      const auto abs = std::filesystem::absolute(fi.path(), ec);
      key = ec ? fi.path().string() : abs.lexically_normal().string();
    }
    if (auto dig = hashes.get_full(key)) {
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

  const QStringList type_order = {QStringLiteral("directory"), QStringLiteral("image"),
                                  QStringLiteral("video"),     QStringLiteral("audio"),
                                  QStringLiteral("document"),  QStringLiteral("archive")};
  for (const QString& cat : type_order) {
    if (!types.contains(cat)) {
      continue;
    }
    AutoChip c;
    c.label = label_for_type(cat);
    c.expression = QStringLiteral("type:%1").arg(cat);
    c.group = AutoChip::Group::Type;
    auto_chips_.push_back(std::move(c));
  }

  // Tagging workflow chips (filter language already supports these).
  if (types.contains(QStringLiteral("image"))) {
    AutoChip c;
    c.label = QStringLiteral("Untagged images");
    c.expression = QStringLiteral("type:image tagged:no");
    c.group = AutoChip::Group::Helper;
    auto_chips_.push_back(std::move(c));
  }
  if (tags_ok) {
    AutoChip c;
    c.label = QStringLiteral("Untagged");
    c.expression = QStringLiteral("tagged:no");
    c.group = AutoChip::Group::Helper;
    auto_chips_.push_back(std::move(c));
  }

  QStringList tag_list = tags_seen.values();
  std::sort(tag_list.begin(), tag_list.end(),
            [](const QString& a, const QString& b) { return a.toLower() < b.toLower(); });
  for (const QString& t : tag_list) {
    AutoChip c;
    c.label = t;
    c.expression = QStringLiteral("tag:%1").arg(t);
    c.group = AutoChip::Group::Tag;
    if (tags_ok) {
      if (auto def = tag_store.get_tag(t.toStdString())) {
        if (def->color.size() >= 7 && def->color[0] == '#') {
          bool ok = false;
          const auto rgb = QString::fromStdString(def->color).mid(1).toUInt(&ok, 16);
          if (ok) {
            c.accent = QColor::fromRgb(static_cast<QRgb>(rgb | 0xff000000u));
          }
        }
        if (!c.accent.isValid()) {
          unsigned h = 2166136261u;
          for (unsigned char ch : def->name) {
            h ^= ch;
            h *= 16777619u;
          }
          c.accent = QColor::fromHsv(static_cast<int>(h % 360), 140, 220);
        }
        if (!def->label.empty()) {
          c.label = QString::fromStdString(def->label);
        }
      }
    }
    auto_chips_.push_back(std::move(c));
  }

  // Sets that have members among the scanned listing (robust path keys), and
  // any set with a member whose parent is the current directory.
  dirtoo::sets::FileSetStore set_store;
  std::string set_err;
  if (set_store.open(dirtoo::sets::FileSetStore::default_path(), &set_err)) {
    QHash<QString, dirtoo::sets::FileSet> sets_seen;
    for (std::size_t i = 0; i < n; ++i) {
      const auto& fi = items[i];
      if (fi.path().empty()) {
        continue;
      }
      if (fi.location().is_archive()) {
        for (const auto& s : set_store.sets_for_path(fi.location().as_url())) {
          sets_seen.insert(QString::fromStdString(s.id), s);
        }
      } else {
        for (const auto& s : set_membership::sets_for_path_robust(set_store, fi.path())) {
          sets_seen.insert(QString::fromStdString(s.id), s);
        }
      }
    }
    // Directory-scoped: member path's parent matches current_directory_.
    if (!current_directory_.isEmpty()) {
      std::unordered_set<std::string> dir_keys;
      set_membership::push_path_forms(dir_keys,
                                      std::filesystem::path{current_directory_.toStdString()});
      for (const auto& s : set_store.list_sets()) {
        if (sets_seen.contains(QString::fromStdString(s.id))) {
          continue;
        }
        for (const auto& m : set_store.members(s.id)) {
          const std::filesystem::path mp{m.path_key};
          std::unordered_set<std::string> parent_keys;
          set_membership::push_path_forms(parent_keys, mp.parent_path());
          bool hit = false;
          for (const auto& pk : parent_keys) {
            if (dir_keys.contains(pk)) {
              hit = true;
              break;
            }
          }
          if (!hit) {
            std::error_code ec;
            if (std::filesystem::equivalent(mp.parent_path(),
                                            std::filesystem::path{current_directory_.toStdString()},
                                            ec)
                && !ec) {
              hit = true;
            }
          }
          if (hit) {
            sets_seen.insert(QString::fromStdString(s.id), s);
            break;
          }
        }
      }
    }
    QList<dirtoo::sets::FileSet> set_list = sets_seen.values();
    std::sort(set_list.begin(), set_list.end(), [](const dirtoo::sets::FileSet& a,
                                                   const dirtoo::sets::FileSet& b) {
      const QString la = a.label.empty() ? QString::fromStdString(a.id.substr(0, 8))
                                         : QString::fromStdString(a.label);
      const QString lb = b.label.empty() ? QString::fromStdString(b.id.substr(0, 8))
                                         : QString::fromStdString(b.label);
      return la.toLower() < lb.toLower();
    });
    for (const auto& s : set_list) {
      AutoChip c;
      c.group = AutoChip::Group::Set;
      c.set_id = QString::fromStdString(s.id);
      if (!s.label.empty()) {
        c.label = QString::fromStdString(s.label);
        c.expression = QStringLiteral("set:%1").arg(c.label);
      } else {
        c.label = QStringLiteral("Set %1").arg(QString::fromStdString(s.id.substr(0, 8)));
        c.expression = QStringLiteral("set:%1").arg(QString::fromStdString(s.id));
      }
      c.accent = set_paint_detail::color_for_set(s);
      auto_chips_.push_back(std::move(c));
    }
  }

  rebuild_buttons();
}

void QuickFilterBar::set_active_expression(const QString& expr)
{
  const QString e = expr.trimmed();
  if (e == active_) {
    return;
  }
  active_ = e;
  // Only update checked state — do not tear down chips on every keystroke.
  if (strip_ == nullptr) {
    return;
  }
  for (auto* btn : strip_->findChildren<QToolButton*>()) {
    const QString bexpr = btn->property("dirtoo_qf_expr").toString();
    if (!bexpr.isEmpty()) {
      const bool on = (bexpr == active_);
      if (btn->isChecked() != on) {
        const QSignalBlocker block(btn);
        btn->setChecked(on);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
        btn->update();
      }
    }
  }
}

void QuickFilterBar::pin_expression(const QString& expr, const QString& label)
{
  const QString e = expr.trimmed();
  if (e.isEmpty()) {
    return;
  }
  for (const auto& p : pins_) {
    if (p.expression == e) {
      return;
    }
  }
  PinnedQuickFilter pin;
  pin.expression = e;
  pin.label = label.trimmed();
  // Default: visible in this directory and its subfolders (not global).
  pin.scope = QuickFilterScope::Subtree;
  if (!current_directory_.isEmpty()) {
    pin.directories = {current_directory_};
  }
  pins_.push_back(std::move(pin));
  save_pins();
  rebuild_buttons();
}

bool QuickFilterBar::pin_visible(const PinnedQuickFilter& pin) const
{
  if (pin.scope == QuickFilterScope::Everywhere) {
    return true;
  }
  if (current_directory_.isEmpty()) {
    return false;
  }
  const QStringList dirs =
      pin.directories.isEmpty() && !current_directory_.isEmpty()
          ? QStringList{current_directory_}
          : pin.directories;
  for (const QString& d : dirs) {
    if (pin.scope == QuickFilterScope::Directory) {
      if (path_equals(current_directory_, d)) {
        return true;
      }
    } else if (pin.scope == QuickFilterScope::Subtree) {
      if (path_is_under(current_directory_, d)) {
        return true;
      }
    }
  }
  return false;
}

void QuickFilterBar::clear_buttons()
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

QToolButton* QuickFilterBar::make_auto_chip(const AutoChip& chip)
{
  auto* btn = new QToolButton(strip_);
  btn->setText(chip.label);
  btn->setCheckable(true);
  btn->setAutoRaise(false); // keep border visible when active
  btn->setToolTip(chip.expression);
  btn->setProperty("dirtoo_qf_expr", chip.expression);
  btn->setChecked(active_ == chip.expression);
  // Active: thick palette(text) outline + bold. Accent fill alone is ambiguous
  // when every set chip already has its own color.
  if (chip.accent.isValid()) {
    const QColor c = chip.accent;
    const QString css = QStringLiteral(
        "QToolButton {"
        "  background-color: rgba(%1,%2,%3,50);"
        "  border: 1px solid rgba(%1,%2,%3,140);"
        "  border-radius: 4px;"
        "  padding: 3px 8px;"
        "  font-weight: 500;"
        "}"
        "QToolButton:checked {"
        "  background-color: rgba(%1,%2,%3,200);"
        "  border: 3px solid palette(text);"
        "  padding: 1px 6px;"
        "  font-weight: 800;"
        "}")
                            .arg(c.red())
                            .arg(c.green())
                            .arg(c.blue());
    btn->setStyleSheet(css);
  } else {
    btn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  border: 1px solid transparent;"
        "  border-radius: 4px;"
        "  padding: 3px 8px;"
        "}"
        "QToolButton:checked {"
        "  background-color: palette(highlight);"
        "  color: palette(highlighted-text);"
        "  border: 3px solid palette(text);"
        "  padding: 1px 6px;"
        "  font-weight: 800;"
        "}"));
  }
  connect(btn, &QToolButton::clicked, this, [this, expression = chip.expression](bool checked) {
    if (checked) {
      emit filter_requested(expression);
    } else if (active_ == expression) {
      emit filter_requested(QString());
    }
  });
  if (chip.group == AutoChip::Group::Set && !chip.set_id.isEmpty()) {
    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    btn->setProperty("dirtoo_qf_set_id", chip.set_id);
    connect(btn, &QWidget::customContextMenuRequested, this,
            [this, set_id = chip.set_id, btn](const QPoint& pos) {
              show_set_menu(set_id, btn->mapToGlobal(pos));
            });
  }
  return btn;
}

QWidget* QuickFilterBar::make_separator()
{
  auto* line = new QFrame(strip_);
  line->setFrameShape(QFrame::VLine);
  line->setFrameShadow(QFrame::Sunken);
  line->setFixedWidth(6);
  line->setStyleSheet(QStringLiteral("QFrame { color: palette(mid); margin: 4px 2px; }"));
  return line;
}

QToolButton* QuickFilterBar::make_pinned_chip(int pin_index)
{
  if (pin_index < 0 || pin_index >= static_cast<int>(pins_.size())) {
    return nullptr;
  }
  const auto& pin = pins_[static_cast<std::size_t>(pin_index)];
  auto* btn = new QToolButton(strip_);
  QString lab = pin.label.isEmpty() ? pin.expression : pin.label;
  if (lab.size() > 28) {
    lab = lab.left(25) + QStringLiteral("…");
  }
  btn->setText(lab);
  btn->setCheckable(true);
  btn->setAutoRaise(true);
  btn->setAutoRaise(false);
  btn->setStyleSheet(QStringLiteral(
      "QToolButton { font-weight: 600; border: 1px solid transparent; border-radius: 4px; "
      "padding: 3px 8px; }"
      "QToolButton:checked { background-color: palette(highlight); color: palette(highlighted-text); "
      "border: 3px solid palette(text); padding: 1px 6px; font-weight: 800; }"));
  QString tip = pin.expression;
  tip += QStringLiteral("\nScope: %1").arg(scope_to_string(pin.scope));
  if (!pin.directories.isEmpty()) {
    tip += QStringLiteral("\n") + pin.directories.join(QStringLiteral("; "));
  }
  tip += QStringLiteral("\nRight-click for options");
  btn->setToolTip(tip);
  btn->setProperty("dirtoo_qf_expr", pin.expression);
  btn->setChecked(active_ == pin.expression);
  connect(btn, &QToolButton::clicked, this, [this, expr = pin.expression](bool checked) {
    if (checked) {
      emit filter_requested(expr);
    } else if (active_ == expr) {
      emit filter_requested(QString());
    }
  });
  btn->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(btn, &QWidget::customContextMenuRequested, this, [this, pin_index, btn](const QPoint& pos) {
    show_pin_menu(pin_index, btn->mapToGlobal(pos));
  });
  return btn;
}

void QuickFilterBar::show_set_menu(const QString& set_id, const QPoint& global_pos)
{
  if (set_id.isEmpty()) {
    return;
  }
  dirtoo::sets::FileSetStore store;
  std::string err;
  if (!store.open(dirtoo::sets::FileSetStore::default_path(), &err)) {
    QMessageBox::warning(this, QStringLiteral("Sets"),
                         QStringLiteral("Cannot open sets database:\n%1")
                             .arg(QString::fromStdString(err)));
    return;
  }
  auto set = store.get_set(set_id.toStdString());
  if (!set) {
    QMessageBox::warning(this, QStringLiteral("Sets"), QStringLiteral("Set no longer exists."));
    emit sets_changed();
    return;
  }

  const QString filter_expr = !set->label.empty()
                                  ? QStringLiteral("set:%1").arg(QString::fromStdString(set->label))
                                  : QStringLiteral("set:%1").arg(QString::fromStdString(set->id));
  const QString set_label = !set->label.empty() ? QString::fromStdString(set->label)
                                                : set_id.left(8);

  QMenu menu(this);
  menu.addAction(QStringLiteral("Filter to this set"), this, [this, filter_expr] {
    emit filter_requested(filter_expr);
  });
  menu.addSeparator();
  menu.addAction(QStringLiteral("Rename…"), this, [this, set_id] {
    dirtoo::sets::FileSetStore st;
    std::string e;
    if (!st.open(dirtoo::sets::FileSetStore::default_path(), &e)) {
      return;
    }
    auto cur = st.get_set(set_id.toStdString());
    const QString current = cur ? QString::fromStdString(cur->label) : QString();
    bool ok = false;
    const QString text = QInputDialog::getText(this, QStringLiteral("Rename set"),
                                               QStringLiteral("Label:"), QLineEdit::Normal,
                                               current, &ok);
    if (!ok) {
      return;
    }
    if (!st.set_label(set_id.toStdString(), text.trimmed().toStdString(), &e)) {
      QMessageBox::warning(this, QStringLiteral("Sets"),
                           QStringLiteral("Could not rename: %1").arg(QString::fromStdString(e)));
      return;
    }
    emit sets_changed();
  });
  menu.addAction(QStringLiteral("Color…"), this, [this, set_id] {
    dirtoo::sets::FileSetStore st;
    std::string e;
    if (!st.open(dirtoo::sets::FileSetStore::default_path(), &e)) {
      return;
    }
    auto cur = st.get_set(set_id.toStdString());
    QColor initial(Qt::gray);
    if (cur && cur->color.size() >= 7 && cur->color[0] == '#') {
      initial = QColor(QString::fromStdString(cur->color));
    } else if (cur) {
      initial = set_paint_detail::color_for_set(*cur);
    }
    const QColor chosen = QColorDialog::getColor(initial, this, QStringLiteral("Set color"));
    if (!chosen.isValid()) {
      return;
    }
    const QString hex = chosen.name(QColor::HexRgb);
    if (!st.set_color(set_id.toStdString(), hex.toStdString(), &e)) {
      QMessageBox::warning(this, QStringLiteral("Sets"),
                           QStringLiteral("Could not set color: %1").arg(QString::fromStdString(e)));
      return;
    }
    emit sets_changed();
  });
  menu.addSeparator();
  menu.addAction(QStringLiteral("Dissolve set…"), this, [this, set_id, set_label] {
    const auto reply = QMessageBox::question(
        this, QStringLiteral("Dissolve set"),
        QStringLiteral("Remove set “%1” and all membership? Files on disk are not deleted.")
            .arg(set_label));
    if (reply != QMessageBox::Yes) {
      return;
    }
    dirtoo::sets::FileSetStore st;
    std::string e;
    if (!st.open(dirtoo::sets::FileSetStore::default_path(), &e)) {
      return;
    }
    if (!st.delete_set(set_id.toStdString(), &e)) {
      QMessageBox::warning(this, QStringLiteral("Sets"),
                           QStringLiteral("Could not dissolve: %1").arg(QString::fromStdString(e)));
      return;
    }
    emit sets_changed();
  });
  menu.exec(global_pos);
}

void QuickFilterBar::show_pin_menu(int pin_index, const QPoint& global_pos)
{
  if (pin_index < 0 || pin_index >= static_cast<int>(pins_.size())) {
    return;
  }
  const auto& pin = pins_[static_cast<std::size_t>(pin_index)];
  QMenu menu(this);
  menu.addAction(QStringLiteral("Edit filter…"), this, [this, pin_index] {
    edit_pin_expression(pin_index);
  });
  menu.addAction(QStringLiteral("Set label…"), this, [this, pin_index] { edit_pin_label(pin_index); });
  menu.addAction(QStringLiteral("Directories…"), this, [this, pin_index] {
    edit_pin_directories(pin_index);
  });
  menu.addSeparator();
  auto* scope_group = new QActionGroup(&menu);
  scope_group->setExclusive(true);
  auto* a_every = menu.addAction(QStringLiteral("Show everywhere"));
  a_every->setCheckable(true);
  a_every->setChecked(pin.scope == QuickFilterScope::Everywhere);
  scope_group->addAction(a_every);
  auto* a_dir = menu.addAction(QStringLiteral("This directory only"));
  a_dir->setCheckable(true);
  a_dir->setChecked(pin.scope == QuickFilterScope::Directory);
  scope_group->addAction(a_dir);
  auto* a_sub = menu.addAction(QStringLiteral("This directory and subdirectories"));
  a_sub->setCheckable(true);
  a_sub->setChecked(pin.scope == QuickFilterScope::Subtree);
  scope_group->addAction(a_sub);
  connect(a_every, &QAction::triggered, this, [this, pin_index] {
    set_pin_scope(pin_index, QuickFilterScope::Everywhere);
  });
  connect(a_dir, &QAction::triggered, this, [this, pin_index] {
    set_pin_scope(pin_index, QuickFilterScope::Directory);
  });
  connect(a_sub, &QAction::triggered, this, [this, pin_index] {
    set_pin_scope(pin_index, QuickFilterScope::Subtree);
  });
  menu.addSeparator();
  menu.addAction(QStringLiteral("Remove"), this, [this, pin_index] { remove_pin(pin_index); });
  menu.exec(global_pos);
}


void QuickFilterBar::edit_pin_expression(int pin_index)
{
  if (pin_index < 0 || pin_index >= static_cast<int>(pins_.size())) {
    return;
  }
  auto& pin = pins_[static_cast<std::size_t>(pin_index)];
  const QString old_expr = pin.expression;
  bool ok = false;
  const QString text = QInputDialog::getText(
      this, QStringLiteral("Edit QuickFilter"),
      QStringLiteral("Filter expression:"), QLineEdit::Normal, pin.expression, &ok);
  if (!ok) {
    return;
  }
  const QString expr = text.trimmed();
  if (expr.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("QuickFilter"),
                         QStringLiteral("Filter expression cannot be empty."));
    return;
  }
  for (int i = 0; i < static_cast<int>(pins_.size()); ++i) {
    if (i != pin_index && pins_[static_cast<std::size_t>(i)].expression == expr) {
      QMessageBox::warning(this, QStringLiteral("QuickFilter"),
                           QStringLiteral("Another pinned filter already uses that expression."));
      return;
    }
  }
  pin.expression = expr;
  save_pins();
  rebuild_buttons();
  if (active_ == old_expr) {
    emit filter_requested(expr);
  }
}

void QuickFilterBar::edit_pin_label(int pin_index)
{
  if (pin_index < 0 || pin_index >= static_cast<int>(pins_.size())) {
    return;
  }
  auto& pin = pins_[static_cast<std::size_t>(pin_index)];
  bool ok = false;
  const QString text = QInputDialog::getText(
      this, QStringLiteral("QuickFilter label"),
      QStringLiteral("Display label (empty = show expression):"), QLineEdit::Normal, pin.label, &ok);
  if (!ok) {
    return;
  }
  pin.label = text.trimmed();
  save_pins();
  rebuild_buttons();
}

void QuickFilterBar::edit_pin_directories(int pin_index)
{
  if (pin_index < 0 || pin_index >= static_cast<int>(pins_.size())) {
    return;
  }
  auto& pin = pins_[static_cast<std::size_t>(pin_index)];

  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("QuickFilter directories"));
  dlg.setMinimumWidth(480);
  auto* layout = new QVBoxLayout(&dlg);
  layout->addWidget(new QLabel(
      QStringLiteral("Directories where this filter appears (when scope is not “everywhere”).\n"
                     "Separate multiple paths with semicolons:\n"
                     "/home/user/Photos;/share/Photos"),
      &dlg));
  auto* edit = new QLineEdit(pin.directories.join(QStringLiteral(";")), &dlg);
  layout->addWidget(edit);
  auto* row = new QHBoxLayout();
  auto* browse = new QToolButton(&dlg);
  browse->setText(QStringLiteral("Add folder…"));
  row->addWidget(browse);
  row->addStretch(1);
  layout->addLayout(row);
  connect(browse, &QToolButton::clicked, &dlg, [edit, &dlg] {
    const QString dir = QFileDialog::getExistingDirectory(&dlg, QStringLiteral("Add directory"));
    if (dir.isEmpty()) {
      return;
    }
    QString cur = edit->text().trimmed();
    if (!cur.isEmpty() && !cur.endsWith(QLatin1Char(';'))) {
      cur += QLatin1Char(';');
    }
    edit->setText(cur + dir);
  });
  if (pin.scope == QuickFilterScope::Everywhere) {
    layout->addWidget(new QLabel(
        QStringLiteral("Note: scope is currently “everywhere” — set Directory or Subtree "
                       "for these paths to take effect."),
        &dlg));
  }
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }
  pin.directories = normalize_dir_list(QStringList{edit->text()});
  save_pins();
  rebuild_buttons();
}

void QuickFilterBar::set_pin_scope(int pin_index, QuickFilterScope scope)
{
  if (pin_index < 0 || pin_index >= static_cast<int>(pins_.size())) {
    return;
  }
  auto& pin = pins_[static_cast<std::size_t>(pin_index)];
  pin.scope = scope;
  // If switching to a path-scoped mode with empty dirs, seed from current location.
  if (scope != QuickFilterScope::Everywhere && pin.directories.isEmpty()
      && !current_directory_.isEmpty()) {
    pin.directories = {current_directory_};
  }
  save_pins();
  rebuild_buttons();
}

void QuickFilterBar::remove_pin(int pin_index)
{
  if (pin_index < 0 || pin_index >= static_cast<int>(pins_.size())) {
    return;
  }
  const QString expr = pins_[static_cast<std::size_t>(pin_index)].expression;
  pins_.erase(pins_.begin() + pin_index);
  save_pins();
  if (active_ == expr) {
    emit filter_requested(QString());
  }
  rebuild_buttons();
}

void QuickFilterBar::rebuild_buttons()
{
  clear_buttons();

  QSet<QString> pinned_exprs;
  for (const auto& p : pins_) {
    pinned_exprs.insert(p.expression);
  }

  auto add_group = [&](AutoChip::Group g) {
    bool any = false;
    for (const auto& c : auto_chips_) {
      if (c.group != g || pinned_exprs.contains(c.expression)) {
        continue;
      }
      if (!any) {
        if (strip_layout_->count() > 0) {
          strip_layout_->addWidget(make_separator());
        }
        any = true;
      }
      strip_layout_->addWidget(make_auto_chip(c));
    }
  };
  add_group(AutoChip::Group::Type);
  add_group(AutoChip::Group::Helper);
  add_group(AutoChip::Group::Tag);
  add_group(AutoChip::Group::Set);

  bool any_pin = false;
  for (int i = 0; i < static_cast<int>(pins_.size()); ++i) {
    if (!pin_visible(pins_[static_cast<std::size_t>(i)])) {
      continue;
    }
    if (!any_pin) {
      if (strip_layout_->count() > 0) {
        strip_layout_->addWidget(make_separator());
      }
      any_pin = true;
    }
    if (auto* btn = make_pinned_chip(i)) {
      strip_layout_->addWidget(btn);
    }
  }
  relayout_height();
}

void QuickFilterBar::load_pins()
{
  pins_.clear();
  QSettings settings(pins_settings_path(), QSettings::IniFormat);
  const int n = settings.beginReadArray(QStringLiteral("pins"));
  for (int i = 0; i < n; ++i) {
    settings.setArrayIndex(i);
    PinnedQuickFilter pin;
    pin.expression = settings.value(QStringLiteral("expression")).toString().trimmed();
    if (pin.expression.isEmpty()) {
      continue;
    }
    pin.label = settings.value(QStringLiteral("label")).toString();
    pin.scope = scope_from_string(settings.value(QStringLiteral("scope")).toString());
    pin.directories =
        normalize_dir_list(settings.value(QStringLiteral("directories")).toStringList());
    pins_.push_back(std::move(pin));
  }
  settings.endArray();
}

void QuickFilterBar::save_pins() const
{
  const QString path = pins_settings_path();
  const QString dir = QFileInfo(path).absolutePath();
  QDir().mkpath(dir);
  QSettings settings(path, QSettings::IniFormat);
  settings.remove(QStringLiteral("pins"));
  settings.beginWriteArray(QStringLiteral("pins"), static_cast<int>(pins_.size()));
  for (int i = 0; i < static_cast<int>(pins_.size()); ++i) {
    settings.setArrayIndex(i);
    const auto& pin = pins_[static_cast<std::size_t>(i)];
    settings.setValue(QStringLiteral("expression"), pin.expression);
    settings.setValue(QStringLiteral("label"), pin.label);
    settings.setValue(QStringLiteral("scope"), scope_to_string(pin.scope));
    settings.setValue(QStringLiteral("directories"), pin.directories);
  }
  settings.endArray();
  settings.sync();
}


int QuickFilterBar::heightForWidth(int width) const
{
  if (strip_layout_ == nullptr) {
    return 36;
  }
  // Pin button sits beside the strip in the outer HBox.
  int pin_w = 0;
  if (pin_btn_ != nullptr) {
    pin_w = pin_btn_->sizeHint().width() + 4;
  }
  const QMargins m = contentsMargins();
  const int strip_w = std::max(1, width - pin_w - m.left() - m.right() - 8);
  const int strip_h = strip_layout_->heightForWidth(strip_w);
  return strip_h + m.top() + m.bottom() + 4;
}

QSize QuickFilterBar::sizeHint() const
{
  const int w = width() > 0 ? width() : 400;
  return QSize(w, heightForWidth(w));
}

QSize QuickFilterBar::minimumSizeHint() const
{
  return QSize(100, 28);
}

void QuickFilterBar::relayout_height()
{
  if (strip_ == nullptr || strip_layout_ == nullptr) {
    return;
  }
  const int w = std::max(1, strip_->width() > 0 ? strip_->width() : width() - 80);
  const int h = strip_layout_->heightForWidth(w);
  strip_->setMinimumHeight(h);
  strip_->setMaximumHeight(h);
  strip_->updateGeometry();
  updateGeometry();
  if (parentWidget() != nullptr) {
    parentWidget()->updateGeometry();
  }
}

void QuickFilterBar::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  relayout_height();
}

} // namespace dirtoo::app

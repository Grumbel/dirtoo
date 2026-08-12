// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window_common.hpp"
#include "filter_history.hpp"

#include <QApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>

namespace dirtoo::app {

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
  // Right-click on an already-selected row must not clear multi-selection.
  if (event->type() == QEvent::MouseButtonPress) {
    auto* me = static_cast<QMouseEvent*>(event);
    if (me->button() == Qt::RightButton) {
      QAbstractItemView* av = nullptr;
      if (tree_view_ != nullptr && obj == tree_view_->viewport()) {
        av = tree_view_;
      } else if (icon_view_ != nullptr && obj == icon_view_->viewport()) {
        av = icon_view_;
      }
      if (av != nullptr && av->selectionModel() != nullptr) {
        const QModelIndex under = av->indexAt(me->pos());
        if (under.isValid() && av->selectionModel()->isSelected(under)) {
          // Swallow the press so the view does not re-select (clearing multi-select).
          // Context menu still arrives via customContextMenuRequested.
          return true;
        }
      }
    }
  }

  if (event->type() == QEvent::MouseButtonRelease) {
    auto* me = static_cast<QMouseEvent*>(event);
    if (me->button() == Qt::MiddleButton && parent_act_ != nullptr) {
      if (auto* tb = qobject_cast<QToolButton*>(obj)) {
        if (tb->defaultAction() == parent_act_) {
          on_parent_new_window();
          return true;
        }
      }
    }
    if (me->button() == Qt::MiddleButton) {
      QModelIndex index;
      if (obj == tree_view_->viewport()) {
        index = tree_view_->indexAt(me->pos());
      } else if (obj == icon_view_->viewport()) {
        index = icon_view_->indexAt(me->pos());
      }
      if (index.isValid()) {
        on_view_middle_click(index);
        return true;
      }
    }
  }

  // Home/End + type-ahead + graphics file-cursor when a file view has focus.
  const bool is_file_view =
      (tree_view_ != nullptr && (obj == tree_view_ || obj == tree_view_->viewport()))
      || (icon_view_ != nullptr && (obj == icon_view_ || obj == icon_view_->viewport()))
      || (graphics_view_ != nullptr
          && (obj == graphics_view_ || obj == graphics_view_->viewport()));
  if (is_file_view && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Home && ke->modifiers() == Qt::NoModifier) {
      jump_to_row(0);
      return true;
    }
    if (ke->key() == Qt::Key_End && ke->modifiers() == Qt::NoModifier) {
      if (model_ != nullptr && model_->rowCount() > 0) {
        jump_to_row(model_->rowCount() - 1);
      }
      return true;
    }
    // Graphics Icons mode: forward cursor keys from the viewport into the view
    // (viewport often holds focus; sendEvent to the view avoids re-entering this
    // filter on graphics_view_ itself).
    if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr
        && obj == graphics_view_->viewport()) {
      const int k = ke->key();
      const bool cursor_key =
          k == Qt::Key_Left || k == Qt::Key_Right || k == Qt::Key_Up || k == Qt::Key_Down
          || k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Escape
          || (k == Qt::Key_Space
              && (ke->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)));
      if (cursor_key) {
        QCoreApplication::sendEvent(graphics_view_, ke);
        return true;
      }
    }
    // Type-ahead: printable text without Ctrl/Alt/Meta.
    // If the filter bar is visible, route keys there (keyword search) instead of
    // opening leap — avoids leap stealing keystrokes intended for the filter.
    if (!(ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        && !ke->text().isEmpty() && ke->text().at(0).isPrint()
        && !ke->text().at(0).isSpace()) {
      const bool filter_visible =
          (filter_row_ != nullptr && filter_row_->isVisible())
          || (filter_edit_ != nullptr && filter_edit_->isVisible());
      if (filter_visible && filter_edit_ != nullptr) {
        filter_edit_->setFocus(Qt::ShortcutFocusReason);
        filter_edit_->insert(ke->text());
        return true;
      }
      if (leap_widget_ != nullptr) {
        leap_widget_->show_with_text(ke->text());
      }
      return true;
    }
  }

  if (obj == search_edit_ && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Escape) {
      stop_search();
      search_session_.active = false;
      search_session_.results.clear();
      if (search_row_ != nullptr) {
        search_row_->hide();
      } else if (search_edit_ != nullptr) {
        search_edit_->hide();
      }
      search_edit_->clear();
      on_directory_changed();
      return true;
    }
  }

  if (obj == filter_edit_ && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Up) {
      if (filter_history_.empty()) {
        return false; // let QLineEdit handle cursor / default
      }
      filter_edit_->setText(filter_history_.older());
      return true;
    }
    if (ke->key() == Qt::Key_Down) {
      if (filter_history_.empty() || filter_history_.index() < 0) {
        return false;
      }
      const QString newer = filter_history_.newer();
      if (newer.isEmpty() && filter_history_.index() < 0) {
        filter_edit_->clear();
      } else if (!newer.isEmpty()) {
        filter_edit_->setText(newer);
      }
      return true;
    }
    if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
      filter_history_.push(filter_edit_->text());
      // Accept the current filter expression and return keyboard focus to the
      // file view so Enter is not "stuck" in the filter line.
      if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
        graphics_view_->setFocus(Qt::OtherFocusReason);
      } else if (QAbstractItemView* view = current_view()) {
        view->setFocus(Qt::OtherFocusReason);
      }
      return true;
    }
  }

  // Hide the filter bar when focus leaves it and the expression is empty
  // (unless pinned). QFocusEvent has no relatedWidget() in Qt6 — use
  // QApplication::focusWidget(), deferred one event-loop turn so the new
  // focus target is settled (help button in the filter row must not hide).
  if (obj == filter_edit_ && event->type() == QEvent::FocusOut) {
    QTimer::singleShot(0, this, [this] {
      if (filter_edit_ == nullptr || filter_pinned_) {
        return;
      }
      if (QWidget* focus = QApplication::focusWidget()) {
        if (filter_row_ != nullptr && filter_row_->isAncestorOf(focus)) {
          return;
        }
        if (focus == filter_edit_) {
          return;
        }
      }
      if (!filter_edit_->text().isEmpty()) {
        return;
      }
      if (show_filter_act_ != nullptr && show_filter_act_->isChecked()) {
        show_filter_act_->setChecked(false);
      }
    });
    return QMainWindow::eventFilter(obj, event);
  }

  // Window / splitter resize changes the viewport without a scrollbar value
  // change; re-request thumbnails for the newly exposed rows.
  if (event->type() == QEvent::Resize) {
    const bool is_view_viewport =
        (tree_view_ != nullptr && obj == tree_view_->viewport())
        || (icon_view_ != nullptr && obj == icon_view_->viewport())
        || (graphics_view_ != nullptr && obj == graphics_view_->viewport());
    if (is_view_viewport) {
      request_thumbnails_for_visible();
    }
  }

  return QMainWindow::eventFilter(obj, event);
}

void MainWindow::on_leap(const QString& text, bool forward, bool from_key)
{
  (void)from_key;
  if (text.isEmpty() || model_ == nullptr) {
    return;
  }
  const int rows = model_->rowCount();
  if (rows <= 0) {
    return;
  }

  int start = 0;
  if (view_mode_ == ViewMode::Icons && graphics_view_ != nullptr) {
    if (graphics_view_->cursor_row() >= 0) {
      start = graphics_view_->cursor_row();
    } else {
      const auto sel = graphics_view_->selected_rows();
      if (!sel.empty()) {
        start = sel.front();
      }
    }
  } else if (auto* view = current_view(); view != nullptr && view->selectionModel() != nullptr) {
    const auto sel = view->selectionModel()->selectedIndexes();
    if (!sel.isEmpty()) {
      start = sel.first().row();
    }
  }

  const QString needle = text.toLower();
  auto matches = [&](int row) {
    const fs::FileInfo* fi = model_->file_at(row);
    if (fi == nullptr) {
      return false;
    }
    return QString::fromStdString(fi->basename()).toLower().startsWith(needle);
  };

  int found = -1;
  if (forward) {
    for (int i = 1; i <= rows; ++i) {
      const int row = (start + i) % rows;
      if (matches(row)) {
        found = row;
        break;
      }
    }
  } else {
    for (int i = 1; i <= rows; ++i) {
      const int row = (start - i + rows * 2) % rows;
      if (matches(row)) {
        found = row;
        break;
      }
    }
  }

  if (found < 0) {
    return;
  }
  jump_to_row(found);
}


} // namespace dirtoo::app

# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2018 Ingo Ruhnke <grumbel@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


from typing import TYPE_CHECKING, Optional, List, cast

import logging

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QPalette, QIcon, QKeySequence
from PyQt5.QtWidgets import (QLineEdit, QShortcut, QWidget,
                             QListWidget, QVBoxLayout, QSizePolicy)

from dirtoo.location import Location

if TYPE_CHECKING:
    from dirtoo.fileview.controller import Controller  # noqa: F401

logger = logging.getLogger(__name__)


class LocationLineEditPopup(QWidget):

    def __init__(self, parent: 'LocationLineEdit') -> None:
        super().__init__(parent,
                         Qt.Window |
                         Qt.WindowStaysOnTopHint |
                         Qt.X11BypassWindowManagerHint |
                         Qt.FramelessWindowHint)

        self._parent = parent
        self._previous_text: Optional[str] = None

        self._build_gui()
        self.listwidget.itemClicked.connect(self._on_item_clicked)

    def _build_gui(self) -> None:
        # Widgets
        self.listwidget = QListWidget()

        # Layout
        vbox = QVBoxLayout()
        # vbox.setSizeConstraint(QLayout.SetMaximumSize)
        vbox.addWidget(self.listwidget)
        vbox.setContentsMargins(0, 0, 0, 0)

        self.listwidget.setSizePolicy(QSizePolicy.Ignored, QSizePolicy.Ignored)

        self.setLayout(vbox)

    def set_completions(self, longest: str, candidates: List[str]) -> None:
        self.listwidget.clear()
        for candidate in candidates:
            self.listwidget.addItem(candidate)

        self._fit_box_to_content()

    def _fit_box_to_content(self) -> None:
        self.listwidget.setMinimumWidth(self.listwidget.sizeHintForColumn(0))

    def on_key_up(self) -> None:
        row = self.listwidget.currentRow()

        if row == -1:
            self._previous_text = self._parent.text()

        row -= 1
        if row >= 0:
            self.listwidget.setCurrentRow(row)

        completion = self.get_current_completion()
        if completion is not None:
            self._parent.setText(completion)

    def on_key_down(self) -> None:
        row = self.listwidget.currentRow()

        if row == -1:
            self._previous_text = self._parent.text()

        row += 1
        if row < self.listwidget.count():
            self.listwidget.setCurrentRow(row)

        completion = self.get_current_completion()
        if completion is not None:
            self._parent.setText(completion)

    def abort_completion(self) -> None:
        self._parent.setText(self._previous_text)
        self._previous_text = None
        self.listwidget.setCurrentRow(-1)

    def get_prefered_height(self) -> int:
        height: int = (self.listwidget.sizeHintForRow(0) * self.listwidget.count() +
                       2 * self.listwidget.frameWidth())
        return min(height, 400)

    def get_current_completion(self) -> Optional[str]:
        row = self.listwidget.currentRow()
        if row == -1:
            return None
        else:
            item = self.listwidget.item(row)
            return cast(str, item.text())

    def _on_item_clicked(self, item):
        self._parent.setText(item.text())
        self._parent.on_return_pressed()


class LocationLineEdit(QLineEdit):

    def __init__(self, controller: 'Controller') -> None:
        super().__init__()

        self._controller = controller
        self.is_unused = True
        self._show_completion_selection = True

        self.returnPressed.connect(self.on_return_pressed)
        self.textEdited.connect(self.on_text_edited)

        self.bookmark_act = self.addAction(QIcon.fromTheme("user-bookmarks"), QLineEdit.TrailingPosition)
        self.bookmark_act.setCheckable(True)
        self.bookmark_act.triggered.connect(self._on_bookmark_triggered)
        self.bookmark_act.setToolTip("Toggle bookmark for this location")

        self._controller.sig_location_changed.connect(self._on_location_changed)
        self._controller.sig_location_changed_to_none.connect(lambda: self._on_location_changed(None))

        self._popup = LocationLineEditPopup(self)

        shortcut = QShortcut(QKeySequence(Qt.CTRL + Qt.Key_G), self)
        shortcut.setContext(Qt.WidgetShortcut)
        shortcut.activated.connect(self._on_escape_press)

        shortcut = QShortcut(QKeySequence(Qt.Key_Escape), self)
        shortcut.setContext(Qt.WidgetShortcut)
        shortcut.activated.connect(self._on_escape_press)

        shortcut = QShortcut(QKeySequence(Qt.Key_Up), self)
        shortcut.setContext(Qt.WidgetShortcut)
        shortcut.activated.connect(self._popup.on_key_up)

        shortcut = QShortcut(QKeySequence(Qt.Key_Down), self)
        shortcut.setContext(Qt.WidgetShortcut)
        shortcut.activated.connect(self._popup.on_key_down)

    def _on_completions(self, longest, candidate) -> None:
        self._popup.on_completions(longest, candidate)
        self._show_completion_selection = True
        self._show_popup()

    def _on_location_changed(self, location: Location):
        if location is not None:
            self.bookmark_act.setEnabled(True)
            if self._controller.has_bookmark():
                self.bookmark_act.setIcon(QIcon.fromTheme("user-bookmarks"))
            else:
                self.bookmark_act.setIcon(QIcon.fromTheme("bookmark-missing"))
        else:
            self.bookmark_act.setEnabled(False)
            self.bookmark_act.setIcon(QIcon())

    def _on_bookmark_triggered(self, checked):
        if self._controller.toggle_bookmark():
            self.bookmark_act.setIcon(QIcon.fromTheme("user-bookmarks"))
        else:
            self.bookmark_act.setIcon(QIcon.fromTheme("bookmark-missing"))

    def _on_escape_press(self):
        if self._controller.location is None:
            self.setText("")
        elif self._popup._previous_text is not None:
            self._popup.abort_completion()
        else:
            self.setText(self._controller.location.as_path())

            p = self.palette()
            p.setColor(QPalette.Text, Qt.black)
            self.setPalette(p)

            self._controller._gui._window.file_view.setFocus()

        self._controller.show_location_buttonbar()
        self._hide_popup()

    def focusInEvent(self, ev) -> None:
        super().focusInEvent(ev)

        if ev.reason() != Qt.ActiveWindowFocusReason and self.is_unused:
            self.setText("")

        p = self.palette()
        p.setColor(QPalette.Text, Qt.black)
        self.setPalette(p)

    def focusOutEvent(self, ev) -> None:
        super().focusOutEvent(ev)

        self._hide_popup()

        if ev.reason() != Qt.ActiveWindowFocusReason:
            self._controller.show_location_buttonbar()

            if self.is_unused:
                self.set_unused_text()

    def on_text_edited(self, text) -> None:
        p = self.palette()

        try:
            location = Location.from_human(text)
            if location.exists():
                p.setColor(QPalette.Text, Qt.black)
            else:
                p.setColor(QPalette.Text, Qt.red)
        except Exception:
            p.setColor(QPalette.Text, Qt.red)

        self.setPalette(p)

        self._controller._path_completion.request_completions(text)

    def set_cursor_to_end(self):
        length = len(self.text())
        self.setCursorPosition(length)

    def on_return_pressed(self) -> None:
        try:
            completition = self._popup.get_current_completion()
            self._popup.hide()
            if completition is not None:
                location = Location.from_path(completition)
            else:
                location = Location.from_human(self.text())
        except Exception:
            logger.warning("unparsable location entered: %s", self.text())
        else:
            self._controller.set_location(location)
            self._popup.hide()

    def set_location(self, location: Location) -> None:
        self.is_unused = False
        p = self.palette()
        p.setColor(QPalette.Text, Qt.black)
        self.setPalette(p)
        self.setText(location.as_human())

    def set_unused_text(self) -> None:
        self.is_unused = True
        p = self.palette()
        p.setColor(QPalette.Text, Qt.gray)
        self.setPalette(p)
        self.setText("no location selected, file list mode is active")

    def keyPressEvent(self, ev):
        if ev.key() == Qt.Key_Backspace or ev.key() == Qt.Key_Delete:
            self._show_completion_selection = False
        else:
            self._show_completion_selection = True

        super().keyPressEvent(ev)

    def _show_popup(self):
        pos = self.parentWidget().mapToGlobal(self.geometry().topLeft())
        self._popup.move(pos.x(),
                         pos.y() + self.height() - 2)
        self._popup.resize(self.width() - 32, self._popup.get_prefered_height())

        if not self._popup.isVisible():
            self._popup.show()

    def _hide_popup(self):
        self._popup.hide()

    def on_completions(self, longest: str, candidate: List[str]) -> None:
        text = self.text()
        if longest != text and self._show_completion_selection:
            self.setText(longest)
            self.setSelection(len(text), len(longest) - len(text))

        self._popup.set_completions(longest, candidate)
        self._show_popup()

    def hideEvent(self, ev) -> None:
        super().hideEvent(ev)
        self._hide_popup()


# EOF #

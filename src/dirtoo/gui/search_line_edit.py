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


from typing import TYPE_CHECKING

from PyQt6.QtCore import Qt, QKeyCombination
from PyQt6.QtGui import QPalette, QKeySequence, QFocusEvent, QShortcut
from PyQt6.QtWidgets import QLineEdit

from dirtoo.image.icon import load_icon

if TYPE_CHECKING:
    from dirtoo.fileview.controller import Controller


class SearchLineEdit(QLineEdit):

    def __init__(self, controller: 'Controller') -> None:
        super().__init__()
        self.controller = controller
        self.is_unused = True
        self.set_unused_text()
        self.returnPressed.connect(self.on_return_pressed)
        self.history: list[str] = []
        self.history_idx = 0

        action = self.addAction(load_icon("window-close"), QLineEdit.ActionPosition.TrailingPosition)
        action.triggered.connect(self._on_reset)
        action.setToolTip("Close the Search")

        shortcut = QShortcut(QKeySequence(QKeyCombination(Qt.Modifier.CTRL, Qt.Key.Key_G)), self)
        shortcut.setContext(Qt.ShortcutContext.WidgetShortcut)
        shortcut.activated.connect(self._on_reset)

        shortcut = QShortcut(QKeySequence(Qt.Key.Key_Escape), self)
        shortcut.setContext(Qt.ShortcutContext.WidgetShortcut)
        shortcut.activated.connect(self._on_reset)

        shortcut = QShortcut(QKeySequence(Qt.Key.Key_Up), self)
        shortcut.setContext(Qt.ShortcutContext.WidgetShortcut)
        shortcut.activated.connect(self.history_up)

        shortcut = QShortcut(QKeySequence(Qt.Key.Key_Down), self)
        shortcut.setContext(Qt.ShortcutContext.WidgetShortcut)
        shortcut.activated.connect(self.history_down)

    def _on_reset(self) -> None:
        self.clear()
        self.history_idx = -1
        self.controller.close_search()

    def on_delete_button(self) -> None:
        self._on_reset()

    def on_return_pressed(self) -> None:
        self.is_unused = False
        self.controller.start_search(self.text())
        self.history.append(self.text())
        self.history_idx = 0

    def focusInEvent(self, ev: QFocusEvent) -> None:
        super().focusInEvent(ev)

        if self.is_unused:
            self.setText("")

        p = self.palette()
        p.setColor(QPalette.ColorRole.Text, Qt.GlobalColor.black)
        self.setPalette(p)

    def focusOutEvent(self, ev: QFocusEvent) -> None:
        super().focusOutEvent(ev)

        if self.text() == "":
            self.is_unused = True
            self.set_unused_text()
        else:
            self.is_unused = False

    def set_unused_text(self) -> None:
        p = self.palette()
        p.setColor(QPalette.ColorRole.Text, Qt.GlobalColor.gray)
        self.setPalette(p)
        self.setText("enter a search pattern here")

    def history_up(self) -> None:
        if self.history != []:
            self.history_idx += 1
            if self.history_idx > len(self.history) - 1:
                self.history_idx = len(self.history) - 1
            self.setText(self.history[len(self.history) - self.history_idx - 1])

    def history_down(self) -> None:
        if self.history != []:
            self.history_idx -= 1
            if self.history_idx < 0:
                self.setText("")
                self.history_idx = 0
            else:
                self.setText(self.history[len(self.history) - self.history_idx - 1])


# EOF #

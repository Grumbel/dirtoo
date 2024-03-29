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


from typing import TYPE_CHECKING, cast, Optional

from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtWidgets import (QLineEdit, QVBoxLayout, QWidget)
from PyQt6.QtGui import QFocusEvent, QKeyEvent, QShowEvent, QHideEvent

if TYPE_CHECKING:
    from dirtoo.fileview.file_view import FileView


class LeapWidget(QWidget):

    sig_leap = pyqtSignal(str, bool, bool)

    def __init__(self, parent: Optional['FileView']) -> None:
        super().__init__(parent,
                         Qt.WindowType.Window |
                         Qt.WindowType.WindowStaysOnTopHint |
                         # Qt.WindowType.X11BypassWindowManagerHint |
                         Qt.WindowType.FramelessWindowHint)

        self._build_gui()

    def _build_gui(self) -> None:
        self._line_edit = QLineEdit(self)
        vbox = QVBoxLayout()
        vbox.addWidget(self._line_edit)
        vbox.setContentsMargins(0, 0, 0, 0)
        self.setLayout(vbox)
        self.resize(200, 40)

        self._line_edit.editingFinished.connect(self.hide)
        self._line_edit.textChanged.connect(self.on_text_changed)

    def showEvent(self, ev: QShowEvent) -> None:
        super().showEvent(ev)
        self.place_widget()

    def place_widget(self) -> None:
        parent = cast('FileView', self.parentWidget())
        pos = parent.mapToGlobal(parent.viewport().geometry().bottomRight())

        self.move(pos.x() - self.width(),
                  pos.y() - self.height())

    def focusOutEvent(self, ev: QFocusEvent) -> None:
        print("focus out")
        super().focusOutEvent(ev)
        self.hide()

    def hideEvent(self, ev: QHideEvent) -> None:
        super().hideEvent(ev)

    def keyPressEvent(self, ev: QKeyEvent) -> None:
        if ev.key() in (Qt.Key.Key_Return, Qt.Key.Key_Enter):
            self.parentWidget().keyPressEvent(ev)
        else:
            super().keyPressEvent(ev)
            if ev.key() == Qt.Key.Key_Escape:
                self.hide()
            elif ev.key() == Qt.Key.Key_Up:
                self.sig_leap.emit(self._line_edit.text(), False, True)
            elif ev.key() == Qt.Key.Key_Down:
                self.sig_leap.emit(self._line_edit.text(), True, True)

    def on_text_changed(self, text: str) -> None:
        self.sig_leap.emit(text, True, False)


# EOF #

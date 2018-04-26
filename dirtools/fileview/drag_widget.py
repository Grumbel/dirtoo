# dirtool.py - diff tool for directories
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


from typing import Optional

from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import QWidget, QApplication
from PyQt5.QtGui import QCursor, QPainter, QFontMetrics


class DragWidget(QWidget):

    """Qt does not allow any dynamic changes to the drag cursor or the
    accompanying pixmap. The DragWidget class is a workaround that
    attaches a temporary widget next to the mouse cursor, specifically
    it puts the words Copy/Link/Move next to the cursor if the proper
    modifier is pressed."""

    def __init__(self, parent: QWidget) -> None:
        super().__init__(parent,
                         Qt.Window |
                         Qt.WindowStaysOnTopHint |
                         Qt.X11BypassWindowManagerHint |
                         Qt.FramelessWindowHint)

        self.resize(48, 20)
        self._mouse_move_timer = self.startTimer(1000 / 60)
        self._text: Optional[str] = None

    def timerEvent(self, ev) -> None:
        if ev.timerId() == self._mouse_move_timer:
            buttons = QApplication.mouseButtons()
            if not buttons & Qt.LeftButton:
                # dispose the widget when the mouse button is released
                self.hide()
                self.killTimer(self._mouse_move_timer)
                self._mouse_move_timer = None
            else:
                # update the mouse position
                pos = QCursor.pos()
                x = pos.x() + 16
                y = pos.y() - 12
                self.move(x, y)

                # FIXME: Qt-bug? .keyboardModifiers() doesn't update until
                # the mouse is moved. .queryKeyboardModifiers() reads
                # straight from the input device and updates properly.
                modifiers = QApplication.queryKeyboardModifiers()

                old_text = self._text

                if modifiers & Qt.ControlModifier and modifiers & Qt.ShiftModifier:
                    self._text = "Link"
                elif modifiers & Qt.ControlModifier:
                    self._text = "Copy"
                elif modifiers & Qt.ShiftModifier:
                    self._text = "Move"
                elif modifiers & Qt.AltModifier:
                    self._text = "Link"
                else:
                    self._text = None

                if old_text != self._text:
                    if self._text is None:
                        # print("Hide")
                        self.hide()
                    else:
                        # print("Show")
                        self.show()
                        self.repaint()

    def paintEvent(self, ev) -> None:
        if self._text is not None:
            painter = QPainter(self)
            font = painter.font()
            fm = QFontMetrics(font)
            width = fm.width(self._text)
            painter.drawText(self.width() // 2 - width // 2, 14, self._text)
            painter.end()


# EOF #

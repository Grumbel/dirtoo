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


from PyQt5.QtCore import Qt, pyqtSignal
from PyQt5.QtWidgets import QToolButton


class ToolButton(QToolButton):
    """QToolButton with the additional ability to attach a secondary
    action to the middle mouse button."""

    sig_middle_click = pyqtSignal()

    def __init__(self) -> None:
        super().__init__()
        self._middle_pressed = False

    def mousePressEvent(self, ev):
        if ev.button() != Qt.MiddleButton:
            if self._middle_pressed:
                ev.ignore()
            else:
                super().mousePressEvent(ev)
        else:
            self.setDown(True)
            self._middle_pressed = True

    def mouseReleaseEvent(self, ev):
        if ev.button() != Qt.MiddleButton:
            if self._middle_pressed:
                ev.ignore()
            else:
                super().mouseReleaseEvent(ev)
        else:
            if self._middle_pressed and self.rect().contains(ev.pos()):
                self.sig_middle_click.emit()

            self.setDown(False)
            self._middle_pressed = False

    def mouseMoveEvent(self, ev):
        if self._middle_pressed:
            if not self.rect().contains(ev.pos()):
                self.setDown(False)
            else:
                self.setDown(True)


# EOF #

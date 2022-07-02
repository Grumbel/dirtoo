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


from typing import Callable, Optional

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QIcon, QMouseEvent
from PyQt5.QtWidgets import QMenu, QAction


class Menu(QMenu):
    """Regular QMenu, but with some functionality hacked in to handle
    middle mouse clicks"""

    def __init__(self, title: Optional[str] = None) -> None:
        if title is None:
            super().__init__()
        else:
            super().__init__(title)

        self._middle_pressed: bool = False

    def mouseReleaseEvent(self, ev: QMouseEvent) -> None:
        if ev.button() == Qt.MiddleButton:
            self._middle_pressed = True

        super().mouseReleaseEvent(ev)

        if ev.button() == Qt.MiddleButton:
            self._middle_pressed = False

    def middle_is_pressed(self) -> bool:
        return self._middle_pressed

    def addDoubleAction(self, icon: Optional[QIcon], title: str,
                        left_func: Callable[[], None],
                        middle_func: Callable[[], None]) -> QAction:

        def callback(menu: Menu = self) -> None:
            if menu.middle_is_pressed():
                middle_func()
            else:
                left_func()

        return self.addAction(icon, title, callback)


# EOF #

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


from PyQt5.QtCore import pyqtSignal, Qt
from PyQt5.QtWidgets import QLabel


class Label(QLabel):

    clicked = pyqtSignal()
    middle_clicked = pyqtSignal()

    def __init__(self, *args) -> None:
        super().__init__(*args)

    def mouseReleaseEvent(self, ev) -> None:
        super().mouseReleaseEvent(ev)

        if not ev.isAccepted() and self.rect().contains(ev.pos()):
            if ev.button() == Qt.LeftButton:
                ev.accept()
                self.clicked.emit()
            elif ev.button() == Qt.MiddleButton:
                ev.accept()
                self.middle_clicked.emit()


# EOF #

# dirtool.py - diff tool for directories
# Copyright (C) 2017 Ingo Ruhnke <grumbel@gmail.com>
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


from PyQt5.QtCore import QRectF
from PyQt5.QtGui import QColor
from PyQt5.QtWidgets import QGraphicsRectItem


class SelectionRect(QGraphicsRectItem):

    def __init__(self):
        super().__init__()

        self.start = None
        self.end = None
        self.setBrush(QColor(128, 128, 255, 96))
        self.setPen(QColor(128, 128, 255, 200))
        self.setZValue(10)
        self.setVisible(False)

    def set_start(self, pos):
        assert self.start is None

        print("selection start", pos)
        self.start = pos

    def update_end(self, pos):
        self.end = pos
        self.setRect(QRectF(self.start, self.end).normalized())
        self.setVisible(True)

    def set_end(self, pos):
        print("selection end", pos)
        self.update_end(pos)
        # do stuff
        self.start = None
        self.end = None
        self.setVisible(False)

    def is_active(self):
        return self.start is not None


# EOF #

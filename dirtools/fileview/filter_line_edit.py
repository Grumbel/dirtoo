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


from typing import List

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QPalette, QIcon
from PyQt5.QtWidgets import QLineEdit


class FilterLineEdit(QLineEdit):

    def __init__(self, controller):
        super().__init__()
        self.controller = controller
        self.is_unused = True
        self.set_unused_text()
        self.returnPressed.connect(self.on_return_pressed)
        self.history: List[str] = []
        self.history_idx = 0

        action = self.addAction(QIcon.fromTheme("edit-delete"), QLineEdit.TrailingPosition)
        action.triggered.connect(self.on_delete_button)
        action.setToolTip("Clear the filter and hide it")

    def keyPressEvent(self, ev):
        super().keyPressEvent(ev)
        if ev.key() == Qt.Key_Escape:
            self.reset()
        elif ev.key() == Qt.Key_Up:
            self.history_up()
        elif ev.key() == Qt.Key_Down:
            self.history_down()
        else:
            self.history_idx = -1

    def reset(self):
        self.clear()
        self.history_idx = -1
        self.controller.set_filter("")
        self.controller.window.hide_filter()
        self.controller.window.thumb_view.setFocus()

    def on_delete_button(self):
        self.reset()

    def on_return_pressed(self):
        self.is_unused = False
        self.controller.set_filter(self.text())
        self.history.append(self.text())
        self.history_idx = 0

    def focusInEvent(self, ev):
        super().focusInEvent(ev)

        if self.is_unused:
            self.setText("")

        p = self.palette()
        p.setColor(QPalette.Text, Qt.black)
        self.setPalette(p)

    def focusOutEvent(self, ev):
        super().focusOutEvent(ev)

        if self.text() == "":
            self.is_unused = True
            self.set_unused_text()
        else:
            self.is_unused = False

    def set_unused_text(self):
        p = self.palette()
        p.setColor(QPalette.Text, Qt.gray)
        self.setPalette(p)
        self.setText("enter a glob search pattern here")

    def history_up(self):
        if self.history != []:
            self.history_idx += 1
            if self.history_idx > len(self.history) - 1:
                self.history_idx = len(self.history) - 1
            self.setText(self.history[len(self.history) - self.history_idx - 1])

    def history_down(self):
        if self.history != []:
            self.history_idx -= 1
            if self.history_idx < 0:
                self.setText("")
                self.history_idx = 0
            else:
                self.setText(self.history[len(self.history) - self.history_idx - 1])


# EOF #
